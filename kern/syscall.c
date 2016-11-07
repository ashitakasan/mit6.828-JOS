#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/syscall.h>
#include <kern/console.h>

/*
  将字符串打印到系统控制台。该字符串正好是“n”个字符长。在内存错误时销毁环境
 */
static void sys_cputs(const char *s, size_t len){
	// 检查用户是否有读取内存的权限[s，s + len]。如果没有，请销毁环境
	

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
