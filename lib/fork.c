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
		panic("Denied copy-on-write 0x%x, env_id 0x%x\n", (uint32_t)addr, thisenv->env_id);

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

	if((r = sys_page_unmap(0, (void *)PFTEMP)) < 0)
		panic("pgfault: sys_page_unmap error %e\n", r);
}

/*
  将虚拟页面 pn (地址 pn * PGSIZE) 映射到目标 envid 同一虚拟地址处
  如果页面是可写的或写时复制的，新映射必须创建写时复制，然后我们的映射必须标记为写时复制
  成功返回 0，错误返回小于 0 或 panic
 */
static int duppage(envid_t envid, unsigned pn){
	// LAB 4
	
	int r;
	void *va = (void *)(pn * PGSIZE);

	if(uvpt[pn] & PTE_SHARE){
		if((r = sys_page_map(0, va, envid, va, PGOFF(uvpt[pn]) & PTE_SYSCALL)) < 0)
			panic("sys_page_map: error %e\n", r);
		return 0;
	}
	if(!(uvpt[pn] & PTE_W) && !(uvpt[pn] & PTE_COW)){
		if((r = sys_page_map(0, va, envid, va, PGOFF(uvpt[pn]))) < 0)
			panic("sys_page_map: error %e\n", r);
		return 0;
	}

	if((uint32_t)va > UTOP)
		panic("page out of UTOP\n");
	if(!(uvpt[pn] & PTE_U))
		panic("page must user accessible\n");

	if((r = sys_page_map(0, va, envid, va, PTE_P | PTE_U | PTE_COW)) < 0)
		panic("sys_page_map: error %e\n", r);

	if((r = sys_page_map(0, va, 0, va, PTE_P | PTE_U | PTE_COW)) < 0)
		panic("sys_page_map: error %e\n", r);

	if((uvpt[pn] & PTE_W) && (uvpt[pn] & PTE_COW))
		panic("duppage: should now set both PTE_W and PTE_COW\n");

	return 0;
}

/*
  具有写时拷贝的用户级 fork；
  适当地设置我们的页面错误处理程序；
  创建子进程；
  将我们的地址空间和页面错误处理程序设置复制到子进程；
  然后将子进程标记为可运行并返回；
  返回：向父进程返回子进程 envid，向子进程返回 0，错误返回小于 0 或 panic

  使用 uvpd, uvpt, duppage；记得在子进程中修改 thisenv；
  用户异常堆栈都不应该被标记为写时复制，因此必须为子用户的异常堆栈分配一个新的页面
 */
envid_t fork(void){
	// LAB 4
	
	envid_t envid = sys_getenvid();
	envid_t child;
	int r;

	set_pgfault_handler(pgfault);

	if((child = sys_exofork()) < 0)
		panic("sys_exofork error %e\n", child);

	if(child == 0){								// 子进程从这里执行，sys_exofork 中子进程返回 0
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	int i, j, pn;
	for(i = 0; i <= PDX(USTACKTOP); i++){
		if(!(uvpd[i] & PTE_P))
			continue;
		for(j = 0; j < NPTENTRIES; j++){
			pn = i * NPTENTRIES + j;
			if(pn >= PGNUM(UXSTACKTOP - PGSIZE))
				break;
			if(uvpt[pn] & PTE_P)
				duppage(child, pn);
		}
	}

	if((r = sys_page_alloc(child, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_alloc: error %e\n", r);

	if((r = sys_page_map(child, (void *)(UXSTACKTOP - PGSIZE), 0, PFTEMP, PTE_P | PTE_U | PTE_W)) < 0)
		panic("sys_page_map: error %e\n", r);

	memmove((void *)PFTEMP, (void *)(UXSTACKTOP - PGSIZE), PGSIZE);

	if((r = sys_page_unmap(0, PFTEMP)) < 0)
		panic("sys_page_unmap: error %e\n", r);

	if((r = sys_env_set_pgfault_upcall(child, thisenv->env_pgfault_upcall)) < 0)
		panic("sys_env_set_pgfault_upcall: error %e\n", r);

	if((r = sys_env_set_status(child, ENV_RUNNABLE)) < 0){
		cprintf("sys_env_set_status: error %e\n", r);
		return -1;
	}

	return child;
}

/*
  Challenge
 */
int sfork(void){

	panic("sfork not implemented");
	return -E_INVAL;
}
