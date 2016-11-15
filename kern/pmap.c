#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/cpu.h>

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
static void mem_init_mp(void);
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
		nextfree = ROUNDUP(nextfree + n, PGSIZE);
		if((uint32_t)nextfree - KERNBASE > (npages * PGSIZE))
			panic("Cannot allocate any more physical memory. Requested %uK, available %uK.\n", 
				(uint32_t) nextfree / 1024, npages * PGSIZE / 1024);
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

	// 初始化一个页目录
	kern_pgdir = (pde_t *)boot_alloc(PGSIZE);
	memset(kern_pgdir, 0, PGSIZE);

	// 作为页表递归插入PD，以形成虚拟地址UVPT的虚拟页表
	// 权限：内核可读，用户可读；UVPT对应的页目录项保存了 操作系统的页表kern_pgdir 的物理地址和权限
	kern_pgdir[PDX(UVPT)] = PADDR(kern_pgdir) | PTE_U | PTE_P;

	// 分配一个包含 npages个PageInfo结构体的数组，保存在 pages 中；
	// 内核使用该数组来跟踪物理页，对于每个物理页，在这个数组中都有对应的 PageInfo结构体；
	// npages 是内存中物理页面个数；使用 memset初始化每个 PageInfo结构体的所有字段为 0。
	
	int pages_size = npages * sizeof(struct PageInfo);
	pages = (struct PageInfo *) boot_alloc(pages_size);
	memset(pages, 0, pages_size);

	// 使 envs 指向一个 NENV 大小的 struct Env 数组

	int envs_size = NENV * sizeof(struct Env);
	envs = (struct Env *) boot_alloc(envs_size);
	memset(envs, 0, envs_size);

	// 现在已经分配了初始化的内核数据结构，并设置了空闲物理页面列表；
	// 一旦这样做，所有进一步的内存管理将通过 page_* function 实现；
	// 特别的，可以使用 boot_map_region 或 page_insert 来映射内存。
	page_init();

	check_page_free_list(1);
	check_page_alloc();
	check_page();

	// 现在开始设置 虚拟内存
	// 在线性地址 UPAGES 处映射用户只读页面
	// 页目录权限：UPAGES 处的映射：内核可读，用户只读；
	// 页面自身：内核读写，用户无权限

	boot_map_region(kern_pgdir, UPAGES, ROUNDUP(sizeof(struct PageInfo) * npages, PGSIZE), PADDR(pages), PTE_U);

	// 映射 envs 数组到线性地址 UENVS，只读权限 （perm = PTE_U | PTE_P）
	// 页目录权限：UPAGES 处的映射：内核可读，用户只读；
	// 页面自身：内核读写，用户无权限

	boot_map_region(kern_pgdir, UENVS, ROUNDUP(envs_size, PGSIZE), PADDR(envs), PTE_U);

	// 使用物理内存，'bootstack'指代内核栈；内核堆栈从 虚拟地址KSTACKTOP向下生长，bootstack = 0xf010d000
	// 我们考虑使用 [KSTACKTOP - PTSIZE, KSTACKTOP] 作为内核栈帧，但是要分成两部分：
	// 	[KSTACKTOP-KSTKSIZE, KSTACKTOP) -- 由物理内存支持
	// 	[KSTACKTOP-PTSIZE, KSTACKTOP-KSTKSIZE) -- 不支持；
	// 		因此如果内核溢出它的栈，会出错而不是写入内存；即保护页；
	// 	权限：内核 RW，用户 无权限

	boot_map_region(kern_pgdir, KSTACKTOP - KSTKSIZE, KSTKSIZE, PADDR(bootstack), PTE_W);

	// 在 KERNBASE 处映射所有的物理内存
	// 例如，虚拟地址 [KERNBASE, 2^32) 应该映射物理地址 [0, 2^32 - KERNBASE)
	// 我们可能没有 2^32 - KERNBASE 的物理内存，但是无论如果我们只要设置映射；
	// 权限：内核 RW，用户 无权限
	
	boot_map_region(kern_pgdir, KERNBASE, 0xfffff000 - KERNBASE, 0, PTE_W);

	// 初始化内存映射的 SMP 相关部分
	mem_init_mp();

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
  修改 kern_dir 中的映射以支持 SMP
  映射区域 [KSTACKTOP-PTSIZE，KSTACKTOP) 中的每个CPU堆栈
 */
static void mem_init_mp(void){
	// 从 KSTACKTOP 开始，映射每个CPU堆栈，最多用于 NCPU 个CPU
	// 对于CPU i，使用 percpu_kstacks[i] 指向的物理内存作为其内核堆栈
	// CPU i的内核堆栈从虚拟地址 kstacktop_i = KSTACKTOP - i *(KSTKSIZE + KSTKGAP) 增长，
	// 并分为两部分，就像在 mem_init 中设置的单个堆栈那样
	// 	* [kstacktop_i - KSTKSIZE, kstacktop_i)  -- 由物理内存支持
	// 	* [kstacktop_i - (KSTKSIZE + KSTKGAP), kstacktop_i - KSTKSIZE)
	// 		-- 不支持，所以如果内核溢出它的堆栈，它会故障而不是覆盖另一个CPU堆栈，被称为“保护页面”
	
	int i = 0;
	uintptr_t kstacktop_i;

	cprintf("mem_init_mp for %d CPUs' kernel stack\n", NCPU);
	for(; i < NCPU; i++){
		kstacktop_i = KSTACKTOP - (i+1) * (KSTKSIZE + KSTKGAP) + KSTKGAP;
		boot_map_region(kern_pgdir, kstacktop_i, KSTKSIZE, PADDR(percpu_kstacks[i]), PTE_W);
	}
}

/*
 页结构初始化，用来跟踪物理页面；pages 数组中，每个物理页面都有一个 PageInfo结构体；
 页面被引用计数，空闲页面保存在链接列表中。
 初始化页结构和空闲内存链表；在此之后，再也不能使用 boot_alloc；
 只能使用 page_alloc 分配函数，通过 page_free_list 来分配和释放物理内存
 */
void page_init(void){
	// LAB 4: 更改您的代码，以将物理页面标记为 MPENTRY_PADDR 为正在使用
	
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
		// npages_basemem = 160, io_hole_size = 96, pgdir_size = 345
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
struct PageInfo *page_alloc(int alloc_flags){
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
 给定一个指向页目录的指针 pgdir，pgdir_walk 返回线性地址的页表项的指针，这需要走两级页表结构；
 相关页表页面可能不存在；如果确实不存在，并且 create == false ，则返回 NULL；
 否则，pgdir_walk使用 page_alloc 分配一个新的页表，
 如果分配失败则返回空，否则，新的页的引用计数加1，该页面被清空，pgdir_walk 返回指向新页面的指针；
 */
pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create){
	pde_t *pde = &pgdir[PDX(va)];						// 根据 va的前10位在pgdir中找到页目录项

	if(!(*pde & PTE_P) && create == false)
		return NULL;
	else if(!(*pde & PTE_P) && create){				// 如果目录项不存在则创建
		struct PageInfo *pi = page_alloc(ALLOC_ZERO);	// 分配一页
		if(pi == NULL)
			return NULL;
		pi->pp_ref++;
		*pde = page2pa(pi) | PTE_P | PTE_U | PTE_W;
	}

	pte_t *pte_base = KADDR(PTE_ADDR(*pde));			// 获取pde的页表基地址
	return &pte_base[PTX(va)];						// 根据 va的次10位 找到页表项
}

/*
 在 pgdir页表 中，映射虚拟地址空间 [va, va+size) 到物理地址 [pa, pa+size)；
 大小是 PGSIZE 的整数倍，虚拟地址va 和物理地址 pa都需要页面对齐；使用权限位 perm|PTE_P；
 该函数仅仅为了设置 UTOP 以上的静态映射；因此，它不能改变映射页面的 pp_ref 字段；
 映射和使用是不一样的，所以这里不需要改变 pages 中页的引用计数
 */
static void boot_map_region(pde_t *pgdir, uintptr_t va, size_t size, physaddr_t pa, int perm){
	pte_t *pte;
	uintptr_t cur_va = va;
	physaddr_t cur_pa = pa;
	uint64_t end_va = va + size;
	uint32_t last_page = 0xfffff000;

	cprintf("boot_map_region va = 0x%x, size = %d*%d, pa = 0x%x\n", va, size/PGSIZE, PGSIZE, pa);

	while(cur_va < end_va){
		pte = pgdir_walk(pgdir, (void *)cur_va, true);	// 建立页表项，分配一个页面
		*pte = PTE_ADDR(cur_pa) | perm | PTE_P;

		if(cur_va >= last_page)
			break;
		cur_va += PGSIZE;
		cur_pa += PGSIZE;
	}
}

/*
 将物理地址 pp 映射到 虚拟地址 va，页表项的权限（低12位）应该设置位 perm | PTE_P
 要求：
 	如果在 va 处已经有一个页面映射，这个页面应该被移除 page_remove()
 	如果有必要，可以根据需要分配页面并插入到 pgdir 中
 	如果插入成功，pp->pp_ref 应该增加
	如果页面以前存在于“va”，则TLB必须失效。
 极端情况的提示：确保考虑到相同的页面在 同一pgdir的同一虚拟地址上重复插入时会发生什么，
 然而，尽量不要在你的代码中区分这种情况，因为这常常导致微妙的错误
 返回：0 成功，E_NO_MEM 如果页面不能被分配
 */
int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm){
	pte_t *pte = pgdir_walk(pgdir, va, true);
	if(pte == NULL)
		return -E_NO_MEM;

	pp->pp_ref++;
	if(*pte & PTE_P)
		page_remove(pgdir, va);

	*pte = page2pa(pp) | perm | PTE_P;
	pgdir[PDX(va)] |= perm;
	return 0;
}

/*
 返回虚拟地址 va处映射的页面，如果 pte_store 不为0，则将该页面的pte的地址存入；
 该函数被 page_remove 使用，可用来验证 系统呼叫参数的页面权限，但不应该被大多数呼叫者使用
 如果没有页面映射 va，则返回 NULL
 */
struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store){
	pte_t *pte = pgdir_walk(pgdir, va, false);
	if(pte == NULL)
		return NULL;
	if(!(*pte & PTE_P))					// 没有页面映射 pte
		return NULL;

	if(pte_store != NULL)
		*pte_store = pte;
	physaddr_t pa = PTE_ADDR(*pte);		// pte 转换为 物理地址
	return pa2page(pa);					// 返回 pa 的 PageInfo
}

/*
 在虚拟地址“va”处取消映射物理页，如果没有物理页映射，什么也不做
 注意：
 	pp->ref 应该减小
 	如果的引用计数为0，物理页面应该被释放
 	va所对应的页表项 应该设置为0
	如果从页表中移除了一项，则TLB 必须失效
 */
void page_remove(pde_t *pgdir, void *va){
	pte_t *pte;
	struct PageInfo *pp = page_lookup(pgdir, va, &pte);
	if(pp == NULL)
		return;

	page_decref(pp);
	tlb_invalidate(pgdir, va);
	*pte &= 0;
}

/*
 使TLB条目无效，但只有当被编辑的页表是处理器当前使用的页表
 */
void tlb_invalidate(pde_t *pgdir, void *va){
	// 仅当我们修改当前地址空间时才刷新该条目
	if(!curenv || curenv->env_pgdir == pgdir)
		invlpg(va);
}

/*
  保留内存映射I/O MMIO 区域中的 size 字节，并在此位置映射 [pa，pa + size]
  返回保留区域的基址。 size 不必须是 PGSIZE 的倍数
 */
void *mmio_map_region(physaddr_t pa, size_t size){
	// 从哪里开始下一个区域。 最初，这是MMIO区域的开始
	// 因为这是静态的，它的值将在调用 mmio_map_region 之间保留（就像在boot_alloc中的nextfree）
	// 注意：由于 check_page() 的测试，在正式调用此函数前， base 会更新两次，然后 base = 0xef803000
	static uintptr_t base = MMIOBASE;

	// 保留虚拟内存从基址开始的 size 字节，将物理页面 [pa, pa+size) 映射到虚拟地址 [base, base+size) 
	// 由于这是设备内存，而不是常规的DRAM，你必须告诉CPU它是不安全的缓存访问这个内存
	// 幸运的是，页表为此目的提供了位; 只需创建 PTE_PCD | PTE_PWT(高速缓存禁用和直写) 的映射以及PTE_W
	// 确保将 size 向上取整到 PGSIZE 整数倍，并处理如果这个保留会溢出 MMIOLIM (可以简单地panic)
	
	size_t n_size = ROUNDUP(size, PGSIZE);
	uintptr_t ret_base = base;
	if(base + n_size >= MMIOLIM)
		panic("mmio_map_region map %d bytes overflow MMIOLIM\n", n_size);

	boot_map_region(kern_pgdir, base, n_size, pa, PTE_W | PTE_PCD | PTE_PWT);
	base += n_size;
	return (void *)ret_base;
}

static uintptr_t user_mem_check_addr;

/*
  检查环境 env 是否允许通过权限 perm | PTE_P 来访问地址范围 [va, va+len)；
  通常“perm”将至少包含PTEU，但这不是必需的；
  'va' 和 'len' 不一定内存页对齐，必须测试每个包含内存地址范围的页面；
  可能会测试 'len/PGSIZE'、'len/PGSIZE + 1'、'len/PGSIZE + 2' 个页面；

  如果（1）地址低于 ULIM，（2）页表给予它许可，用户程序可以访问虚拟地址；
  这些是你应该在这里实现的测试。
  如果存在错误，请将 'user_mem_check_addr' 变量设置为第一个错误的虚拟地址；
  返回 0 表示用户程序可以访问该内存地址范围；否则返回 -E_FAULT
 */
int user_mem_check(struct Env *env, const void *va, size_t len, int perm){
	uintptr_t start_va = (uintptr_t)ROUNDDOWN(va, PGSIZE);
	uintptr_t end_va = (uintptr_t)ROUNDUP(va + len, PGSIZE);

	pte_t *pte;
	perm |= PTE_P;

	while(start_va < end_va){
		pte = pgdir_walk(env->env_pgdir, (void *)start_va, 0);
		if(!pte || start_va >= ULIM || (*pte & perm) != perm){
			user_mem_check_addr = start_va < (uintptr_t)va ? (uintptr_t)va : start_va;
			return -E_FAULT;
		}
		start_va += PGSIZE;
	}
	return 0;
}

/*
  检查环境 env 是否允许通过 perm | PTE_U | PTE_P 权限来访问内存范围 [va, va+len)；
  如果可以，直接返回；
  如果不能，就结束该 env，如果该 env 是当前运行环境，该函数可能不会返回
 */
void user_mem_assert(struct Env *env, const void *va, size_t len, int perm){
	if(user_mem_check(env, va, len, perm | PTE_U) < 0){
		cprintf("[%08x] user_mem_check assertion failure for va %08x\n", 
				env->env_id, user_mem_check_addr);
		env_destroy(env);			// 可能不会返回
	}
}

/**
 * 检查功能
 */

/*
 检查page_free_list上的页面是否合理
 */
static void check_page_free_list(bool only_low_memory){
	struct PageInfo *pp;
	unsigned pdx_limit = only_low_memory ? 1 : NPDENTRIES;
	int nfree_basemem = 0, nfree_extmem = 0;
	char *first_free_page;

	if (!page_free_list)
		panic("'page_free_list' is a null pointer!");

	if (only_low_memory) {
		// Move pages with lower addresses first in the free
		// list, since entry_pgdir does not map all pages.
		struct PageInfo *pp1, *pp2;
		struct PageInfo **tp[2] = { &pp1, &pp2 };
		for (pp = page_free_list; pp; pp = pp->pp_link) {
			int pagetype = PDX(page2pa(pp)) >= pdx_limit;
			*tp[pagetype] = pp;
			tp[pagetype] = &pp->pp_link;
		}
		*tp[1] = 0;
		*tp[0] = pp2;
		page_free_list = pp1;
	}

	// if there's a page that shouldn't be on the free list,
	// try to make sure it eventually causes trouble.
	for (pp = page_free_list; pp; pp = pp->pp_link)
		if (PDX(page2pa(pp)) < pdx_limit)
			memset(page2kva(pp), 0x97, 128);

	first_free_page = (char *) boot_alloc(0);
	for (pp = page_free_list; pp; pp = pp->pp_link) {
		// check that we didn't corrupt the free list itself
		assert(pp >= pages);
		assert(pp < pages + npages);
		assert(((char *) pp - (char *) pages) % sizeof(*pp) == 0);

		// check a few pages that shouldn't be on the free list
		assert(page2pa(pp) != 0);
		assert(page2pa(pp) != IOPHYSMEM);
		assert(page2pa(pp) != EXTPHYSMEM - PGSIZE);
		assert(page2pa(pp) != EXTPHYSMEM);
		assert(page2pa(pp) < EXTPHYSMEM || (char *) page2kva(pp) >= first_free_page);
		// (new test for lab 4)
		assert(page2pa(pp) != MPENTRY_PADDR);

		if (page2pa(pp) < EXTPHYSMEM)
			++nfree_basemem;
		else
			++nfree_extmem;
	}

	assert(nfree_basemem > 0);
	assert(nfree_extmem > 0);
}

/*
 检查物理页面分配（page_alloc(), page_free(), and page_init()）
 */
static void check_page_alloc(void){
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	int nfree;
	struct PageInfo *fl;
	char *c;
	int i;

	if (!pages)
		panic("'pages' is a null pointer!");

	// check number of free pages
	for (pp = page_free_list, nfree = 0; pp; pp = pp->pp_link)
		++nfree;

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(page2pa(pp0) < npages*PGSIZE);
	assert(page2pa(pp1) < npages*PGSIZE);
	assert(page2pa(pp2) < npages*PGSIZE);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// free and re-allocate?
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);
	assert(!page_alloc(0));

	// test flags
	memset(page2kva(pp0), 1, PGSIZE);
	page_free(pp0);
	assert((pp = page_alloc(ALLOC_ZERO)));
	assert(pp && pp0 == pp);
	c = page2kva(pp);
	for (i = 0; i < PGSIZE; i++)
		assert(c[i] == 0);

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	// number of free pages should be the same
	for (pp = page_free_list; pp; pp = pp->pp_link)
		--nfree;
	assert(nfree == 0);

	cprintf("check_page_alloc() succeeded!\n");
}

/*
 检查虚拟地址空间的内核部分是否已大致正确设置（通过 mem_init()）
 这个函数不测试每个角落的情况，但它是一个相当不错的健全检查
 */
static void check_kern_pgdir(void){
	uint32_t i, n;
	pde_t *pgdir;

	pgdir = kern_pgdir;

	// check pages array
	n = ROUNDUP(npages*sizeof(struct PageInfo), PGSIZE);
	for (i = 0; i < n; i += PGSIZE)
		assert(check_va2pa(pgdir, UPAGES + i) == PADDR(pages) + i);

	// check envs array (lab3)
	n = ROUNDUP(NENV*sizeof(struct Env), PGSIZE);
	for (i = 0; i < n; i += PGSIZE)
		assert(check_va2pa(pgdir, UENVS + i) == PADDR(envs) + i);

	// check phys mem
	for (i = 0; i < npages * PGSIZE; i += PGSIZE)
		assert(check_va2pa(pgdir, KERNBASE + i) == i);

	// check kernel stack
	// (updated in lab 4 to check per-CPU kernel stacks)
	for (n = 0; n < NCPU; n++) {
		uint32_t base = KSTACKTOP - (KSTKSIZE + KSTKGAP) * (n + 1);
		for (i = 0; i < KSTKSIZE; i += PGSIZE)
			assert(check_va2pa(pgdir, base + KSTKGAP + i)
				== PADDR(percpu_kstacks[n]) + i);
		for (i = 0; i < KSTKGAP; i += PGSIZE)
			assert(check_va2pa(pgdir, base + i) == ~0);
	}

	// check PDE permissions
	for (i = 0; i < NPDENTRIES; i++) {
		switch (i) {
		case PDX(UVPT):
		case PDX(KSTACKTOP-1):
		case PDX(UPAGES):
		case PDX(UENVS):
		case PDX(MMIOBASE):
			assert(pgdir[i] & PTE_P);
			break;
		default:
			if (i >= PDX(KERNBASE)) {
				assert(pgdir[i] & PTE_P);
				assert(pgdir[i] & PTE_W);
			} else
				assert(pgdir[i] == 0);
			break;
		}
	}
	cprintf("check_kern_pgdir() succeeded!\n");
}

/*
 该函数返回含有地址 va的由页面目录 pgdir 定义的页面的物理地址，硬件通常为我们执行此功能
 我们定义自己的版本来检查 check_kern_pgdir() 函数，它不应该被到处使用
 */
static physaddr_t check_va2pa(pde_t *pgdir, uintptr_t va){
	pte_t *p;

	pgdir = &pgdir[PDX(va)];
	if (!(*pgdir & PTE_P))
		return ~0;
	p = (pte_t*) KADDR(PTE_ADDR(*pgdir));
	if (!(p[PTX(va)] & PTE_P))
		return ~0;
	return PTE_ADDR(p[PTX(va)]);
}

/*
 检查 page_insert, page_remove 等
 */
static void check_page(void){
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	void *va;
	uintptr_t mm1, mm2;
	int i;
	extern pde_t entry_pgdir[];

	// should be able to allocate three pages
	pp0 = pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));

	assert(pp0);
	assert(pp1 && pp1 != pp0);
	assert(pp2 && pp2 != pp1 && pp2 != pp0);

	// temporarily steal the rest of the free pages
	fl = page_free_list;
	page_free_list = 0;

	// should be no free memory
	assert(!page_alloc(0));

	// there is no page allocated at address 0
	assert(page_lookup(kern_pgdir, (void *) 0x0, &ptep) == NULL);

	// there is no free memory, so we can't allocate a page table
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) < 0);

	// free pp0 and try again: pp0 should be used for page table
	page_free(pp0);
	assert(page_insert(kern_pgdir, pp1, 0x0, PTE_W) == 0);
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	assert(check_va2pa(kern_pgdir, 0x0) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp0->pp_ref == 1);

	// should be able to map pp2 at PGSIZE because pp0 is already allocated for page table
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// should be no free memory
	assert(!page_alloc(0));

	// should be able to map pp2 at PGSIZE because it's already there
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);

	// pp2 should NOT be on the free list
	// could happen in ref counts are handled sloppily in page_insert
	assert(!page_alloc(0));

	// check that pgdir_walk returns a pointer to the pte
	ptep = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(PGSIZE)]));
	assert(pgdir_walk(kern_pgdir, (void*)PGSIZE, 0) == ptep+PTX(PGSIZE));

	// should be able to change permissions too.
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W|PTE_U) == 0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp2));
	assert(pp2->pp_ref == 1);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U);
	assert(kern_pgdir[0] & PTE_U);

	// should be able to remap with fewer permissions
	assert(page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W) == 0);
	assert(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_W);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should not be able to map at PTSIZE because need free page for page table
	assert(page_insert(kern_pgdir, pp0, (void*) PTSIZE, PTE_W) < 0);

	// insert pp1 at PGSIZE (replacing pp2)
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W) == 0);
	assert(!(*pgdir_walk(kern_pgdir, (void*) PGSIZE, 0) & PTE_U));

	// should have pp1 at both 0 and PGSIZE, pp2 nowhere, ...
	assert(check_va2pa(kern_pgdir, 0) == page2pa(pp1));
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	// ... and ref counts should reflect this
	assert(pp1->pp_ref == 2);
	assert(pp2->pp_ref == 0);

	// pp2 should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp2);

	// unmapping pp1 at 0 should keep pp1 at PGSIZE
	page_remove(kern_pgdir, 0x0);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == page2pa(pp1));
	assert(pp1->pp_ref == 1);
	assert(pp2->pp_ref == 0);

	// test re-inserting pp1 at PGSIZE
	assert(page_insert(kern_pgdir, pp1, (void*) PGSIZE, 0) == 0);
	assert(pp1->pp_ref);
	assert(pp1->pp_link == NULL);

	// unmapping pp1 at PGSIZE should free it
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(check_va2pa(kern_pgdir, 0x0) == ~0);
	assert(check_va2pa(kern_pgdir, PGSIZE) == ~0);
	assert(pp1->pp_ref == 0);
	assert(pp2->pp_ref == 0);

	// so it should be returned by page_alloc
	assert((pp = page_alloc(0)) && pp == pp1);

	// should be no free memory
	assert(!page_alloc(0));

	// forcibly take pp0 back
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// check pointer arithmetic in pgdir_walk
	page_free(pp0);
	va = (void*)(PGSIZE * NPDENTRIES + PGSIZE);
	ptep = pgdir_walk(kern_pgdir, va, 1);
	ptep1 = (pte_t *) KADDR(PTE_ADDR(kern_pgdir[PDX(va)]));
	assert(ptep == ptep1 + PTX(va));
	kern_pgdir[PDX(va)] = 0;
	pp0->pp_ref = 0;

	// check that new page tables get cleared
	memset(page2kva(pp0), 0xFF, PGSIZE);
	page_free(pp0);
	pgdir_walk(kern_pgdir, 0x0, 1);
	ptep = (pte_t *) page2kva(pp0);
	for(i=0; i<NPTENTRIES; i++)
		assert((ptep[i] & PTE_P) == 0);
	kern_pgdir[0] = 0;
	pp0->pp_ref = 0;

	// give free list back
	page_free_list = fl;

	// free the pages we took
	page_free(pp0);
	page_free(pp1);
	page_free(pp2);

	// test mmio_map_region
	mm1 = (uintptr_t) mmio_map_region(0, 4097);
	mm2 = (uintptr_t) mmio_map_region(0, 4096);
	// check that they're in the right region
	assert(mm1 >= MMIOBASE && mm1 + 8096 < MMIOLIM);
	assert(mm2 >= MMIOBASE && mm2 + 8096 < MMIOLIM);
	// check that they're page-aligned
	assert(mm1 % PGSIZE == 0 && mm2 % PGSIZE == 0);
	// check that they don't overlap
	assert(mm1 + 8096 <= mm2);
	// check page mappings
	assert(check_va2pa(kern_pgdir, mm1) == 0);
	assert(check_va2pa(kern_pgdir, mm1+PGSIZE) == PGSIZE);
	assert(check_va2pa(kern_pgdir, mm2) == 0);
	assert(check_va2pa(kern_pgdir, mm2+PGSIZE) == ~0);
	// check permissions
	assert(*pgdir_walk(kern_pgdir, (void*) mm1, 0) & (PTE_W|PTE_PWT|PTE_PCD));
	assert(!(*pgdir_walk(kern_pgdir, (void*) mm1, 0) & PTE_U));
	// clear the mappings
	*pgdir_walk(kern_pgdir, (void*) mm1, 0) = 0;
	*pgdir_walk(kern_pgdir, (void*) mm1 + PGSIZE, 0) = 0;
	*pgdir_walk(kern_pgdir, (void*) mm2, 0) = 0;

	cprintf("check_page() succeeded!\n");
}

/*
 在 kern_pgdir 安装后，检查 page_insert, page_remove 等
 */
static void check_page_installed_pgdir(void){
	struct PageInfo *pp, *pp0, *pp1, *pp2;
	struct PageInfo *fl;
	pte_t *ptep, *ptep1;
	uintptr_t va;
	int i;

	// check that we can read and write installed pages
	pp1 = pp2 = 0;
	assert((pp0 = page_alloc(0)));
	assert((pp1 = page_alloc(0)));
	assert((pp2 = page_alloc(0)));
	page_free(pp0);
	memset(page2kva(pp1), 1, PGSIZE);
	memset(page2kva(pp2), 2, PGSIZE);
	page_insert(kern_pgdir, pp1, (void*) PGSIZE, PTE_W);
	assert(pp1->pp_ref == 1);
	assert(*(uint32_t *)PGSIZE == 0x01010101U);
	page_insert(kern_pgdir, pp2, (void*) PGSIZE, PTE_W);
	assert(*(uint32_t *)PGSIZE == 0x02020202U);
	assert(pp2->pp_ref == 1);
	assert(pp1->pp_ref == 0);
	*(uint32_t *)PGSIZE = 0x03030303U;
	assert(*(uint32_t *)page2kva(pp2) == 0x03030303U);
	page_remove(kern_pgdir, (void*) PGSIZE);
	assert(pp2->pp_ref == 0);

	// forcibly take pp0 back
	assert(PTE_ADDR(kern_pgdir[0]) == page2pa(pp0));
	kern_pgdir[0] = 0;
	assert(pp0->pp_ref == 1);
	pp0->pp_ref = 0;

	// free the pages we took
	page_free(pp0);

	cprintf("check_page_installed_pgdir() succeeded!\n");
}
