#ifndef JOS_KERN_PMAP_H
#define JOS_KERN_PMAP_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#include <inc/memlayout.h>
#include <inc/assert.h>

extern char bootstacktop[], bootstack[];

extern struct PageIngo *pages;
extern size_t npages;

extern pde_t *kern_pgdir;

/*
 这个宏需要一个内核虚拟地址，指向一个高于 KERNBASE的地址，
 KERNBASE映射到机器的物理地址上最大的 256M，返回对应的物理地址。
 */
#define	PADDR(kva)	_paddr(__FILE__, __LINE__, kva)

static inline physaddr_t _paddr(const char *file, int line, void *kva){
	if((uint32_t)kva < KERNBASE)
		_panic(file, line, "PADDR called with invalid kva %08lx", kva);
	return (physaddr_t)kva - KERNBASE;
}

/*
 这个宏定义接收一个物理地址，返回其对应的内核虚拟地址
 */
#define	KADDR(pa)	_kaddr(__FILE__, __LINE__, pa)

static inline void *_kaddr(const char *file, int line, physaddr_t pa){
	if(PGNUM(pa) >= npages)
		_panic(file, line, "KADDR called with invalid pa %08lx", pa);
	return (void *)(pa + KERNBASE);
}


enum {
	ALLOC_ZERO = 1 << 0,		// 对于page_alloc，零返回物理页
};


void mem_init(void);
void page_init(void);

struct PageInfo *page_alloc(int alloc_flags);
void page_free(struct PageInfo *pp);

int page_insert(pde_t *pgdir, struct PageInfo *pp, void *va, int perm);
void page_remove(pde_t *pgdir, void *va);

struct PageInfo *page_lookup(pde_t *pgdir, void *va, pte_t **pte_store);
void page_decref(struct PageInfo *pp);

void tlb_invalidate(pde_t *pgdir, void *va);

// 将一个PageInfo结构体转换到对应的物理地址
static inline physaddr_t page2pa(struct PageInfo *pp){
	return (pp - pages) << PGSHIFT;
}

// 将一个物理地址转换到对应的PageInfo结构体
static inline sturct PageInfo *pa2page(physaddr_t pa){
	if(PGNUM(pa) >= npages)
		panic("pa2page called with invalid pa");
	return &pages[PGNUM(pa)];
}

// 将一个PageInfo结构体转换到对应的 内核虚拟地址
static inline void *page2kva(struct PageInfo *pp){
	return KADDR(page2pa(pp));
}

pte_t *pgdir_walk(pde_t *pgdir, const void *va, int create);

#endif
/* !JOS_KERN_PMAP_H */
