#include <inc/lib.h>

/*
  panic 被称为无法解决的致命错误。 
  它打印“panic：<message>”，然后造成断点异常，这导致JOS进入JOS内核监视器
 */
void _panic(const char *file, int line, const char *fmt, ...){
	va_list ap;
	va_start(ap, fmt);

	cprintf("[%08x] user panic in %s at %s:%d: ", 
			sys_getenvid(), binaryname, file, line);
	vcprintf(fmt, ap);
	cprintf("\n");

	// 产生一个断点异常
	while(1)
		asm volatile("int3");
}
