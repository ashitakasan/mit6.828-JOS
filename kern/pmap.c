#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>

// 这些变量被 i386_detect_memory() 函数设置
size_t npages;						// 物理内存量
static size_t npages npages_basemem;	// 基础内存量

// 这些变量被 mem_init() 设置
pde_t *kern_pgdir;					// 内核初始化 页目录
struct PageInfo *pages;				// 物理页状态数组
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
 这个物理内存分配器仅仅在OS设置它的虚拟物理地址时使用，page_alloc()是真正的分配器；
 如果 n>0；分配足够的相邻物理页面保存 n字节，不会初始会内存，返回 内核虚拟地址；
 如果 n=0；返回下一个没有被分配的空白页的地址；
 如果内存超出，boot_alloc 应该发出异常；这个函数应该仅仅被用在初始化阶段，page_free_list 没有设置之前。
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
	
	

	// 
}




