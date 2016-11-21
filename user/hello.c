// hello, world
#include <inc/lib.h>

void rec(int n){
	if(n > 0)
		cprintf("result n = %d\n", n);
	rec(n + 1);
}

void
umain(int argc, char **argv)
{
	cprintf("hello, world\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
	rec(1);
}
