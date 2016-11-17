#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>

/*
  将字符串打印到系统控制台。该字符串正好是“n”个字符长。在内存错误时销毁环境
 */
static void sys_cputs(const char *s, size_t len){
	// 检查用户是否有读取内存的权限[s，s + len]。如果没有，请销毁环境
	
	user_mem_assert(curenv, (void *)s, len, PTE_U);

	// 打印用户提供的字符串
	cprintf("%.*s", len, s);
}

/*
  从系统控制台读取字符而不阻塞。返回输入的字符，如果没有输入等待，则返回0
 */
static int sys_cgetc(void){
	return cons_getc();
}

/*
  返回当前环境envid
 */
static envid_t sys_getenvid(void){
	return curenv->env_id;
}

/*
  销毁给定的环境（可能是当前运行的环境）
  成功返回 0，错误返回小于 0：
  	如果环境envid不存在或调用者没有更改envid的权限，返回 -E_BAD_ENV，
 */
static int sys_env_destroy(envid_t envid){
	int r;
	struct Env *e;

	if((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if(e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);

	env_destroy(e);
	return 0;
}

/*
  取消调度当前环境并选择不同的运行环境
 */
static void sys_yield(void){
	sched_yield();
}

/*
  分配新环境
  返回新环境的 envid，错误，返回小于 0：
  	-E_NO_FREE_ENV 如果没有空闲环境存在
  	-E_NO_MEM 内存耗尽
 */
static envid_t sys_exofork(void){
	// 用 kern/env.c 中的 env_alloc() 创建一个新的环境
	// 它应该保留为 env_alloc 创建它，除了状态设置为 ENV_NOT_RUNNABLE ，
	// 	并且寄存器从当前环境复制 -- 但是有所调整，以便 sys_exofork 返回 0
	//LAB 4
	
	struct Env *e;
	int ret;
	if((ret = env_alloc(&e, curenv->env_id)) < 0)
		return ret;
	
	e->env_status = ENV_NOT_RUNNABLE;
	e->env_tf = curenv->env_tf;
	e->env_tf.tf_regs.reg_eax = 0;			// 子进程将从这里运行，返回 0
	return e->env_id;
}

/*
  将 envid 的 env_status 设置为 status ，它必须是 ENV_RUNNABLE 或 ENV_NOT_RUNNABLE
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV，如果环境 envid 当前不存在，或者调用者没有更改 envid 的权限
  	-E_INVAL，如果状态不是环境的有效状态
 */
static int sys_env_set_status(envid_t envid, int status){
	// 使用 kern/env.c 中的 envid2env 函数将 envid 转换为 struct Env
	// 您应该将envid2env的第三个参数设置为 1，这将检查当前环境是否有权设置envid的状态
	// LAB 4
	
	struct Env *e;
	int ret;
	if((ret = envid2env(envid, &e, 1)) < 0)
		return ret;

	if(status != ENV_RUNNABLE && status != ENV_NOT_RUNNABLE)
		return -E_INVAL;
	e->env_status = status;
	return 0;
}

/*
  通过修改 envid 对应的 struct Env 的 env_pgfault_upcall 字段来设置页面错误 upcall
  当 envid 导致页面错误时，内核将故障记录推送到异常堆栈，然后分支到 func
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV，如果环境 envid 当前不存在，或者调用者没有更改 envid 的权限
 */
static int sys_env_set_pgfault_upcall(envid_t envid, void *func){
	// LAB 4
	
	struct Env *e;
	int ret;
	if((ret = envid2env(envid, &e, 1)) < 0)
		return ret;

	if(!func)
		return -E_INVAL;
	user_mem_assert(e, func, 4, PTE_U);

	e->env_pgfault_upcall = func;
	return 0;
}

/*
  分配一页内存，并在 envid 的地址空间中将它映射到 va，并具有 perm 权限
  页面的内容设置为 0，如果页面已经映射到 va，则该页面将被取消映射为副作用
  perm - PTE_U | PTE_P 必须设置，PTE_AVAIL | PTE_W 可能不被设置，但不能设置其他位
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV 如果环境 envid 当前不存在，或者调用者没有更改 envid 的权限
  	-E_INVAL 如果 va >= UTOP，或者 va 没有页面对齐
  	-E_INVAL 如果如果 perm 不合适
  	-E_NO_MEM 如果没有内存来分配新页面，或者分配任何必要的页表
 */
static int sys_page_alloc(envid_t envid, void *va, int perm){
	// 此函数是来自 kern/pmap.c 的 page_alloc() 和 page_insert() 的包装
	// 你写的大部分新代码应该是检查参数的正确性
	// 如果 page_insert() 失败，请记住释放您分配的页面
	// LAB 4
	
	struct Env *e;
	struct PageInfo *pp;
	int ret;

	if((ret = envid2env(envid, &e, 1)) < 0)					// 检查 envid
		return ret;

	if((uint32_t)va >= UTOP || ((uint32_t)va % PGSIZE) != 0)	// 检查 va
		return -E_INVAL;
	if((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || 
		(perm & ~(PTE_U | PTE_P | PTE_AVAIL | PTE_W)) != 0)		// 检查 perm
		return -E_INVAL;

	if((pp = page_alloc(ALLOC_ZERO)) == 0)
		return -E_NO_MEM;

	if((ret = page_insert(e->env_pgdir, pp, va, perm)) < 0){
		page_free(pp);
		return ret;
	}

	return 0;
}

/*
  将 srcenvid 的地址空间中的 srcva 映射为 dstenvid 的地址空间中的 dstva，其权限为 perm
  perm 具有与 sys_page_alloc 中相同的限制，除了它也不能向只读页面授予写访问权限
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV，如果环境 envid 当前不存在，或者调用者没有更改 envid 的权限
  	-E_INVAL，如果 va >= UTOP，或者 va 没有页面对齐
  	-E_INVAL，如果 srcva 没有在 srcenvid 映射
  	-E_INVAL，如果如果 perm 不合适
  	-E_NO_MEM 如果没有内存来分配新页面，或者分配任何必要的页表
 */
static int sys_page_map(envid_t srcenvid, void *srcva, envid_t dstenvid, void *dstva, int perm){
	// 此函数是来自 kern pmap.c 的 page_lookup() 和 page_insert() 的包装
	// 同样，你写的大多数新代码应该是检查参数的正确性
	// 使用 page_lookup() 的第三个参数来检查页面上的当前权限
	// LAB 4
	
	struct Env *srce, *dste;
	struct PageInfo *pp;
	int ret;
	pte_t *pte;

	if((ret = envid2env(srcenvid, &srce, 1)) < 0)
		return ret;
	if((ret = envid2env(dstenvid, &dste, 1)) < 0)
		return ret;

	if((uint32_t)srcva >= UTOP || ((uint32_t)srcva % PGSIZE) != 0 || 
		(uint32_t)dstva >= UTOP || ((uint32_t)dstva % PGSIZE) != 0){
		cprintf("sys_page_map: invalid boundary or page-aligned\n");
		return -E_INVAL;
	}

	if((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || 
		(perm & ~(PTE_U | PTE_P | PTE_AVAIL | PTE_W)) != 0){	// 第一次检查 perm
		cprintf("sys_page_map: invalid perm\n");
		return -E_INVAL;
	}

	if((pp = page_lookup(srce->env_pgdir, srcva, &pte)) == 0){
		cprintf("sys_page_map: page not found\n");
		return -E_INVAL;
	}

	if(((*pte & PTE_W) == 0) && (perm & PTE_W))					// 第二次检查 perm
		return -E_INVAL;

	if((ret = page_insert(dste->env_pgdir, pp, dstva, perm)) < 0)
		return ret;

	return 0;
}

/*
  在 envid 的地址空间中的 va 处取消映射内存页面。如果未映射页面，则该函数将静默成功
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV，如果环境 envid 当前不存在，或者调用者没有更改 envid 的权限
  	-E_INVAL，如果 va >= UTOP，或者 va 没有页面对齐
 */
static int sys_page_unmap(envid_t envid, void *va){
	// 该函数是 page_remove() 的包装
	// LAB 4
	
	struct Env *e;
	int ret;

	if((ret = envid2env(envid, &e, 1)) < 0)
		return ret;

	if((uint32_t)va >= UTOP || ((uint32_t)va % PGSIZE) != 0)
		return -E_INVAL;

	page_remove(e->env_pgdir, va);
	return 0;
}

/*
  尝试发送 valua 到目标 env envid
  如果 srcva < UTOP，则还发送当前映射到 srcva 的页面，以便接收者获得同一页面的重复映射；
  如果目标未被阻塞等待IPC，则发送失败，返回值为 -E_IPC_NOT_RECV；
  发送也可能由于下面列出的其他原因失败

  否则，发送成功，目标的 ipc 字段将更新如下：
	env_ipc_recving 设置为 0 以阻止将来的发送
	env_ipc_from 设置为发送 envid
	env_ipc_value 设置为 value 参数
	如果页面被传输，env_ipc_perm 设置为 perm ，否则为 0
  目标环境再次标记为可运行，从暂停的 sys_ipc_recv 系统调用返回 0 (提示：sys_ipc_recv 函数是否返回?)

  如果发送方想要发送页面但接收方不接受，则不传送页面映射，但不发生错误
  ipc只在没有错误时发生
  成功返回 0，错误返回小于 0：
  	-E_BAD_ENV，如果环境envid当前不存在 (无需检查权限)
  	-E_IPC_NOT_RECV，如果 envid 当前未在 sys_ipc_recv 中阻塞，或另一个环境管理先发送
	-E_INVAL，如果 srcva < UTOP 但是 srcva 没有页面对齐
	-E_INVAL，如果 srcva < UTOP 但是权限不合适 (参考 sys_page_alloc)
	-E_INVAL，如果 srcva < UTOP 但是 srcva 没有在调用者的地址空间中映射
	-E_INVAL，如果 (perm & PTE_W)，但是 srcva 在当前环境的地址空间中是只读的
	-E_NO_MEM，如果 如果没有足够的内存来映射 srcva 在 envid 的地址空间
 */
static int sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm){
	// LAB 4
	
	struct Env *e;
	int ret;

	if((ret = envid2env(envid, &e, 0)) < 0)
		return ret;

	if(e->env_ipc_recving == 0)
		return -E_IPC_NOT_RECV;

	if((uint32_t)srcva < UTOP && (uint32_t)e->env_ipc_dstva < UTOP){
		if(((uint32_t)srcva % PGSIZE) != 0)
			return -E_INVAL;

		// PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W  may not be set, no other bits set
		if((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P) || 
			(perm & ~(PTE_U | PTE_P | PTE_AVAIL | PTE_W)) != 0)
			return -E_INVAL;
		
		pte_t *pte;
		struct PageInfo *pp;

		if((pp = page_lookup(curenv->env_pgdir, srcva, &pte)) == 0)
			return -E_INVAL;

		if((perm & PTE_W) && ((*pte & PTE_W) == 0))
			return -E_INVAL;

		if((ret = page_insert(e->env_pgdir, pp, e->env_ipc_dstva, perm)) < 0)
			return ret;
	}

	// check complete, now transfer
	e->env_ipc_recving = 0;
	e->env_ipc_from = curenv->env_id;
	e->env_ipc_value = value;
	e->env_ipc_perm = perm;

	e->env_status = ENV_RUNNABLE;
	return 0;
}

/*
  阻塞直到 value 准备就绪。使用 struct Env 的 env_ipc_recving 和 env_ipc_dstva 
  字段来接收您想要接收的记录，标记自己不可运行，然后放弃 CPU；
  如果 dstva < UTOP，那么你会接收一个数据页面，'dstva' 是发送的页面应该映射的虚拟地址；
  此函数仅在错误时返回，但系统调用 syscall 最终将在成功时返回 0
  错误返回小于 0：-E_INVAL 如果 dstva < UTOP 但是 dstva 没有页面对齐
 */
static int sys_ipc_recv(void *dstva){
	// LAB 4
	if((uint32_t)dstva < UTOP){
		if(((uint32_t)dstva % PGSIZE) != 0)
			return -E_INVAL;
	}

	curenv->env_ipc_value = 0;
	curenv->env_ipc_perm = 0;
	curenv->env_ipc_from = 0;

	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_recving = 1;
	curenv->env_status = ENV_NOT_RUNNABLE;

	// sched_yield();
	return 0;
}

/*
  调度到正确的内核函数，传递参数
 */
int32_t syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5){
	// 调用对应于 'syscall' 参数的函数。 返回任何适当的返回值
	
	switch (syscallno) {
		case SYS_cputs:
			sys_cputs((char *)a1, (size_t)a2);
			break;
		case SYS_cgetc:
			return sys_cgetc();
		case SYS_getenvid:
			return sys_getenvid();
		case SYS_env_destroy:
			return sys_env_destroy((envid_t)a1);
		case SYS_yield:
			sys_yield();
			break;
		case SYS_exofork:
			return sys_exofork();
		case SYS_env_set_status:
			return sys_env_set_status((envid_t)a1, a2);
		case SYS_env_set_pgfault_upcall:
			return sys_env_set_pgfault_upcall((envid_t)a1, (void *)a2);
		case SYS_page_alloc:
			return sys_page_alloc((envid_t)a1, (void *)a2, a3);
		case SYS_page_map:
			return sys_page_map((envid_t)a1, (void *)a2, (envid_t)a3, (void *)a4, a5);
		case SYS_page_unmap:
			return sys_page_unmap((envid_t)a1, (void *)a2);
		case SYS_ipc_try_send:
			return sys_ipc_try_send((envid_t)a1, a2, (void *)a3, (unsigned)a4);
		case SYS_ipc_recv:
			return sys_ipc_recv((void *)a1);
		default:
			return -E_INVAL;
	}
	return 0;
}
