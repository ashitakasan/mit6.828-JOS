#include <inc/string.h>
#include <inc/lib.h>

void cputchar(int ch){
	char c = ch;
	// 与标准Unix的putchar不同，cputchar函数 总是 输出到系统控制台
	sys_cputs(&c, 1);
}

int getchar(void){
	int r;
	while((r = sys_cgetc()) == 0)
		sys_yield();
	return r;
}
