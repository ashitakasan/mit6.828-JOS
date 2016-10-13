#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// 这些变量被 i386_detect_memory() 函数设置
size_t npages;							// 物理内存总量
static size_t npages_basemem;			// 基础内存量

// 这些变量被 mem_init() 设置
pde_t *kern_pgdir;						// 内核初始化 页目录
struct PageInfo *pages;					// 物理页状态数组
static struct PageInfo *page_free_list;	// 物理页空闲列表


/*
 检测机器上的物理内存设置
 */

// 从 nvram 读取内存信息
static int nvram_read(int r){
	return mc146818_read(r) | (mc146818_read(r + 1) << 8);
}

// i386 内存大小信息检测
static void i386_detect_memory(void){
	size_t basemem, extmem, ext16mem, totalmem;

	// 使用CMOS调用来测量可用的基本和扩展内存，CMOS调用 返回 kbytes
	basemem = nvram_read(NVRAM_BASELO);
	extmem = nvram_read(NVRAM_EXTLO);
	ext16mem = nvram_read(NVRAM_EXT16LO) * 64;

	// 开始计算基本内存和扩展内存的物理页面数量
	if(ext16mem)
		totalmem = 16 * 1024 + ext16mem;
	else if(extmem)
		totalmem = 1 * 1024 + extmem;
	else
		totalmem = basemem;

	npages = totalmem / (PGSIZE / 1024);
	npages_basemem = basemem / (PGSIZE / 1024);

	cprintf("Physical memory: %uK available, base = %uK, extended = %uK\n",
			totalmem, basemem, totalmem - basemem);
}

/*
 设置 UTOP 以上的内存映射
 */
static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm);
static void check_page_free_list(bool only_low_memort);
static void check_page_alloc(void);
static void check_kern_pgdir(void);
static physaddr_t check_va2pa(pde_t *pgdir, uintptr_t va);
static void check_page(void);
static void check_page_installed_pgdir(void);


/*
 这个物理内存分配器仅仅在OS设置它的虚拟物理地址时使用，page_alloc() 才是真正的分配器；
 如果 n>0；分配足够的相邻物理页面保存 n字节，不会初始会内存，返回 内核虚拟地址；
 如果 n=0；返回下一个没有被分配的空白页的地址；
 如果内存超出，boot_alloc 应该发出异常；这个函数应仅仅被用在初始化阶段，page_free_list 未设置之前。
 */
static void *boot_alloc(uint32_t n){
	static char *nextfree;			// 空闲内存的下一字节的虚拟地址
	char *result;

	// 第一次使用时初始化 nextfree，'end'是个特殊的符号，被链接器生成，
	// 他指向 kernel的bss段的结尾，即链接器不会分配任何kernel代码和全局变量的 第一个虚拟地址。
	if(!nextfree){
		extern char end[];
		nextfree = ROUNDUP((char *)end, PGSIZE);
	}

	// 分配一块足够容纳 n字节的内存，然后更新 nextfree，确保 nextfree 是 PGSIZE的整数倍

	result = nextfree;
	if(n > 0){
		uint32_t size = ROUNDUP(n, PGSIZE);
		nextfree += size * PGSIZE;
		if((uint32_t)nextfree - KERNBASE > npages * PGSIZE)
			panic("Out of memory !\n");
	}
	return result;
}

/*
 设置二级页表：kern_pgdir 是页表的根的现行虚拟地址；
 这个函数仅仅设置地址空间的内核部分（ addr >= UTOP），地址空间的用户部分稍后设置。
 */
void mem_init(void){
	uint32_t cr0;
	size_t n;

	// 查询当前机器有多少可用内存页数
	i386_detect_memory();

	panic("mem_init: This function is developing\n");

	// 初始化一个页目录
	kern_pgdir = (pde_t *)boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);

	// 作为页表递归插入PD，以形成虚拟地址UVPT的虚拟页表

	// 页表权限：内核可读，用户可读；UVPT 向后保存了 操作系统的页表kern_pgdir
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	// 分配一个包含 npages个PageInfo结构体的数组，保存在 pages 中；
	// 内核使用该数组来跟踪物理页，对于每个物理页，在这个数组中都有对应的 PageInfo结构体；
	// npages 是内存中物理页面个数；使用 memset初始化每个 PageInfo结构体的所有字段为 0。
	
	int pages_size = npages * sizeof(struct PageInfo);
	pages = (struct PageInfo *) boot_alloc(pages_size);
	memset(pages, 0, pages_size);

	// 现在已经分配了初始化的内核数据结构，并设置了空闲物理页面列表；
	// 一旦这样做，所有进一步的内存管理将通过 page_* function 实现；
	// 特别的，可以使用 boot_map_region 或 page_insert 来映射内存。
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	check_page();

	// 现在开始设置 虚拟内存
	// 在线性地址 UPAGES 处映射用户只读页面
	// 权限：UPAGES新镜像：内核可读，用户可读；页面自身：内核读写，用户无权限
	


	// 使用物理内存，'bootstack'指代内核栈；内核堆栈从 虚拟地址KSTACKTOP向下生长，
	// 我们考虑使用 [KSTACKTOP - PTSIZE, KSTACKTOP] 作为内核栈帧，但是要分成两部分：
	// 	[KSTACKTOP-KSTKSIZE, KSTACKTOP) -- 由物理内存支持
	// 	[KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- 不支持；
	// 		因此如果内核溢出它的栈，会出错而不是写入内存；即保护页；
	// 	权限：内核 RW，用户 无权限



	// 在 KERNBASE 处映射所有的物理内存
	// 例如，虚拟地址 [KERNBASE, 2^32) 应该映射物理地址 [0, 2^32 - KERNBASE)
	// 我们可能没有 2^32 - KERNBASE 的物理内存，但是无论如果我们只要设置映射；
	// 权限：内核 RW，用户 无权限
	


	// 检查初始页目录是否已正确设置
	check_kern_pgdir();

	// 从从最小入口页目录切换到我们刚刚创建的完整的kern_dir页表
	// 现在我们的指令应该指向 KERNBASE 到 KERNBASE+4MB 之间的某处，其由两个页表以相同的方式映射
	// 如果机器此时重新启动，可能设置的kern_dir有错误
	lcr3(PADDR(kern_pgdir));

	check_page_free_list(0);

	// entry.S 在 cr0 中设置了非常重要的标志，包括启用分页；这里配置剩下的需要的标志位
	cr0 = rcr0();
	cr0 |= CR0_PE|CR0_PG|CR0_AM|CR0_WP|CR0_NE|CR0_MP;
	cr0 &= ~(CR0_TS|CR0_EM);
	lcr0(cr0);

	// 一些更多的检查，只有在安装kern_dir之后才可能
	check_page_installed_pgdir();
}

/*
 跟踪物理页面；pages 数组中，每个物理页面都有一个 PageInfo结构体；
 页面被引用计数，空闲页面保存在链接列表中。
 初始化页结构和空闲内存链表；在此之后，再也不能使用 boot_alloc；
 只能使用 page_alloc 分配函数，通过 page_free_list 来分配和释放物理内存
 */
void page_init(void){
	// 这段代码将所有的物理页面标记为空闲；然而这不是真的；这些内存是空闲的：
	// 1) 标记物理页面 0在使用中，这样，我们保留实模式IDT和BIOS结构，以防万一我们需要它们
	// 2) 剩余的基本内存是空闲的，[PGSIZE, npages_basemem * PGSIZE)
	// 3) 然后是IO洞，[IOPHYSMEM, EXTPHYSMEM)，绝不能被分配
	// 4) 扩展内存，[EXTPHYSMEM, ...)；其中有些是使用的，有些是空闲的；
	// 		考虑内核在物理内存的什么地方，哪些页面已经被页表和其他数据结构使用了
	// 
	// 不要实际接触与空闲页对应的物理内存

	size_t i;
	// I/O 空洞，从 IOPHYSMEM 即 npages_basemem (640KB)开始，到 EXTPHYSMEM 即 1MB 结束
	size_t io_hole_size = ROUNDUP((EXTPHYSMEM - IOPHYSMEM), PGSIZE) / PGSIZE;
	// 页目录和页表项保存的地方，这里一定是 PGSIZE 整数倍，从 EXTPHYSMEM 即 1MB处开始
	size_t pgdir_size = ((size_t)boot_alloc(0) - KERNBASE) / PGSIZE;

	for(i = 1; i < npages; i++){
		if(i == 0 || i == 7 || (i >= npages_basemem && i < npages_basemem + io_hole_size + pgdir_size)){
			pages[i].pp_ref = 1;
			continue;
		}
		pages[i].pp_ref = 0;
		pages[i].pp_link = page_free_list;
		page_free_list = &pages[i];
	}
}

/*
 分配一个物理页面
 如果 alloc_flag & ALLOC_ZERO，则填充整个物理页面为 '\0'；
 不要增加页面的引用计数，调用者必须做这个，明确增加或者通过 page_insert；
 确保将分配页面的tp_link字段设置为NULL，以便page_free可以检查双重释放错误；
 如果内存溢出则返回空，可以使用 page2kva 和 memset
 */
static PageInfo *page_alloc(int alloc_flags){
	struct PageInfo *result;
	if(page_free_list == NULL)			// 说明内存溢出
		return NULL;

	result = page_free_list;
	if(result->pp_ref != 0)
		panic("Current page's pp->pp_ref is nonzero !\n");
	page_free_list = result->pp_link;
	result->pp_link = NULL;

	if(alloc_flags & ALLOC_ZERO)
		memset(page2kva(result), '\0', PGSIZE);		// 填充整个物理页面为 '\0'

	return result;
}

/*
 释放一个页面到 page_free_list 中，只有当 pp->pp_ref 达到 0 时才能调用
 */
void page_free(struct PageInfo *pp){
	// 如果 pp->pp_ref 不是 0 或者 pp->pp_link 不是 NULL，应该调用 panic
	if(pp->pp_ref != 0)
		panic("The pp->pp_ref is nonzero, shouldn't call page_free !\n");
	if(pp->pp_link != NULL)
		panic("The pp->pp_link is not NULL, shouldn't call page_free !\n");

	pp->pp_link = page_free_list;
	page_free_list = pp;
}

/*
 减小一个页面的引用计数，如果引用计数为 0，则调用 page_free
 */
void page_decref(struct PageInfo *pp){
	if(--pp->pp_ref == 0)
		page_free(pp);
}

/*
 给一个指向页目录的指针 pgdir，pgdir_walk 返回指向线性地址页表项的指针，这需要走两级页表结构；
 相关页表页面可能不存在；如果确实不存在，并且 create == false ，则返回 NULL；
 否则，pgdir_walk使用 page_alloc 分配一个新的页表，
 如果分配失败则返回空，否则，新的页的引用计数加1，该页面被清空，pgdir_walk 返回指向新页面的指针；
 */
pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create){
	
	return NULL;
}


