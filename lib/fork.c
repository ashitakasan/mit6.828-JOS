#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW 标记了写时复制页面表条目，它是显式分配给用户进程 (PTE_AVAIL) 的位之一
#define PTE_COW		0x800

/*
  自定义页面错误处理程序 - 如果故障页面是写入时复制，映射在我们自己的专用可写副本中
 */
static void pgfault(struct UTrapframe *utf){
	void *addr = (void *)utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// 检查故障访问是 (1) 写，和 (2) 写入时复制页，如果不是，panic；
	// 在 uvpt 处使用只读页表映射
	// LAB 4

	if((err & FEC_WR) == 0)
		panic("Denied write 0x%x: err %x\n", (uint32_t)addr, err);

	pte_t pte = uvpt[PGNUM((uint32_t)addr)];				// 去 uvpt 上访问，就相当于访问了自己的 页表

	if((pte & PTE_COW) == 0)
		panic("Denied copy-on-write 0x%x, env_id 0x%x\n", (uint32_t)addr, curenv->env_id);

	// 分配新页面，将其映射到临时位置 (PFTEMP)，
	// 将数据从旧页面复制到新页面，然后将新页面移动到旧页面的地址；
	// 你应该进行三个系统调用
	// LAB 4
	
	void *align_va = (void *)ROUNDDOWN((uint32_t)addr, PGSIZE);
	if((r = sys_page_alloc(0, (void *)PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("pgfault: sys_page_alloc error %e\n", r);

	memmove((void *)PFTEMP, align_va, PGSIZE);

	if((r = sys_page_map(0, (void *)PFTEMP, 0, align_va, PTE_P | PTE_U | PTE_W)) < 0)
		panic("pgfault: sys_page_map error %e\n", r);
}

/*
  将虚拟页面 pn (地址 pn * PGSIZE) 映射到目标 envid 同一虚拟地址处
  新映射必须创建写时复制，然后我们的映射也必须标记为写时复制
  成功返回 0，错误返回小于 0 或 panic
 */
static int duppage(envid_t envid, unsigned pn){
	// LAB 4
	
	void *va - (void *)(pn * PGSIZE);
	pde_t *src_pgdir = 
}

/*
  
 */
envid_t fork(void){

	panic("fork not implemented");
}

/*
  Challenge
 */
int sfork(void){

	panic("sfork not implemented");
	return -E_INVAL;
}
