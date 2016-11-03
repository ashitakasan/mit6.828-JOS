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

 */
static void sys_cputs(const char *s, size_t len){
	cprintf("%.*s", len, s);
}

/*

 */
static int sys_cgetc(void){
	return cons_getc();
}

/*

 */
static envid_t sys_getenvid(void){
	return curenv->env_id;
}

/*

 */
static int sys_env_destroy(env_id envid){
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

 */
int32_t syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5){
	
	panic("syscall not implemented");

	switch(syscallno){
		default:
			return -E_INVAL;
	}
}
