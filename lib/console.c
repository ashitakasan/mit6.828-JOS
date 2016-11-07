#include <inc/string.h>
#include <inc/lib.h>

void cputchar(int ch){
	char c = ch;
	sys_cputs(&c, 1);
}

int getchar(void){
	int r;
	while((r = sys_cgetc()) == 0)
		;
	return r;
}
