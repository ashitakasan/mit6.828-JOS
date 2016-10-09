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
 这个物理内存分配器仅仅在OS设置它的虚拟物理地址时使用，page_alloc()是真正的分配器
 */
static void boot_alloc(uint32_t n){

}



