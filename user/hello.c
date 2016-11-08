// hello, world
#include <inc/lib.h>

void recursion(int n){
	cprintf("recursion level %d\n", n);
	recursion(++n);
}

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
	recursion(0);
}
