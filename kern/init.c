#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>

// 测试栈回溯功能
void test_backtrace(int x){
	cprintf("entering test_backtrace %d\n", x);
	if(x > 0)
		test_backtrace(x-1);
	else
		mon_backtrace(0, 0, 0);
	cprintf("leaving test_backtrace %d\n", x);
}

/*
 内核初始化
 */
void i386_init(void){
	extern char edata[], end[];

	// 执行其他任务之前，完成ELF加载过程；
	// 清除我们的程序的未初始化的全局数据（BSS）部分；
	// 这将确保所有静态/全局变量开始于零
	memset(edata, 0, end - edata);

	// 初始化终端，在此之前不能调用 cprintf
	cons_init();

	cprintf("6.828 decimal is %o octal!\n", 6828);

	// 测试栈回溯功能
	// test_backtrace(5);

	// Lab 2 内存管理初始化
	mem_init();

	// Lab 3 用户环境初始化
	env_init();
	trap_init();

#if defined(TEST)
	// 不要直接用这里 - 通过分级脚本使用
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	ENV_CREATE(user_hello, ENV_TYPE_USER);
#endif
	
	// 现在只有一个用户环境
	env_run(&envs[0]);

	// 陷入内核监控
	// while(1)
	// 	monitor(NULL);
}

// 可变的 panicstr包含第一次调用 panic的参数，
// 使用一个标志来表示内核已经调用过 panic
const char *panicstr;

/*
 发生无法解决的致命错误时 _panic 被调用，它打印"panic: mesg"，然后进入内核监控
 */
void _panic(const char *file, int line, const char *fmt, ...){
	va_list ap;

	if(panicstr)
		goto dead;
	panicstr = fmt;

	// 要特别确保设备处于合理状态
	asm volatile("cli; cld");

	va_start(ap, fmt);
	// 内核发生错误的 文件+行数
	cprintf("kernel panic at %s:%d: ", file, line);
	vcprintf(fmt, ap);		// 打印可变参数
	cprintf("\n");
	va_end(ap);

dead:
	while(1)					// 陷入内核监控
		monitor(NULL);
}

/*
 内核发生警告
 */
void _warn(const char *file, int line, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	// 内核发生错误的 文件+行数
	cprintf("kernel warning at %s:%d: ", file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
