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
	// 	并且寄存器集从当前环境复制 -- 但是有所调整，以便 sys_exofork 返回 0
	//LAB 4
	

	panic("sys_exofork not implemented");
}

/*
  
 */
static int sys_env_set_status(envid_t envid, int status){

	panic("sys_env_set_status not implemented");
}

/*
  
 */
static int sys_env_set_pgfault_upcall(envid_t envid, void *func){

	panic("sys_env_set_pgfault_upcall not implemented");
}

/*
  
 */
static int sys_page_alloc(envid_t envid, void *va, int perm){

	panic("sys_page_alloc not implemented");
}

/*

 */
static int sys_page_map(envid_t srcenvid, void *srcva, envid_t dstenvid, void *dstva, int perm){

	panic("sys_page_map not implemented");
}

/*
  
 */
static int sys_page_unmap(envid_t envid, void *va){

	panic("sys_page_unmap not implemented");
}

/*
  
 */
static int sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm){

	panic("sys_ipc_try_send not implemented");
}

/*
  
 */
static sys_ipc_recv(void *dstva){

	panic("sys_ipc_recv not implemented");
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
		default:
			return -E_INVAL;
	}
	return 0;
}
