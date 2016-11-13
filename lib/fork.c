#include <inc/string.h>
#include <inc/lib.h>

#define PTE_COW		0x800

/*
  
 */
static void pgfault(struct UTrapframe *utf){

	panic("pgfault not implemented");
}

/*
  
 */
static int duppage(envid_t envid, unsigned pn){

	panic("duppage not implemented");
}

/*
  
 */
envid_t fork(void){

	panic("fork not implemented");
}

/*
  
 */
int sfork(void){

	panic("sfork not implemented");
}
