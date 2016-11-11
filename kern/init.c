#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/monitor.h>
#include <kern/console.h>
#include <kern/pmap.h>
#include <kern/kclock.h>
#include <kern/env.h>
#include <kern/trap.h>
#include <kern/sched.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static void boot_aps(void);

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

	// LAB 4 多处理器初始化函数
	mp_init();
	lapic_init();

	// LAB 4 多任务初始化函数
	pic_init();

	// 在唤醒 APs 之前获取大内核锁
	

	// 启动非引导CPU
	boot_aps();

#if defined(TEST)
	// 不要直接用这里 - 通过分级脚本使用
	ENV_CREATE(TEST, ENV_TYPE_USER);
#else
	ENV_CREATE(user_primes, ENV_TYPE_USER);
#endif
	
	sched_yield();

	// 现在只有一个用户环境
	// env_run(&envs[0]);

	// 陷入内核监控
	// while(1)
	// 	monitor(NULL);
}

// 当boot_aps正在引导给定的CPU时，它将应该由 mpentry.S
// 加载的每个核心的堆栈指针传递给该变量中的那个CPU
void *mpentry_kstack;

/*
  启动非引导 (AP) 处理器
 */
static void boot_aps(void){
	extern unsigned char mpentry_start[], mpentry_end[];
	void *code;
	struct CpuInfo *c;

	// 在 MPENTRY_PADDR 处将入口代码 mpentry.S 写入未使用的存储器
	// MPENTRY_PADDR 仅仅是在 CPU 引导期间使用的代码存放地址
	code = KADDR(MPENTRY_PADDR);
	memmove(code, mpentry_start, mpentry_end - mpentry_start);

	// 每次引导 一个 AP
	for(c = cpus; c < cpus + ncpu; c++){
		if(c == cpus + cpunum())
			continue;

		// 告诉 mpentry.S 使用什么栈， 每个 CPU 有独立的栈
		mpentry_kstack = percpu_kstacks[c - cpus] + KSTKSIZE;
		// 在 mpentry_start 地址处启动 CPU
		lapic_startap(c->cpu_id, PADDR(code));
		// 等待CPU在 mp_main() 中完成一些基本设置，
		while(c->cpu_status != CPU_STARTED)
			;
	}
}

/*
  AP 的设置代码
 */
void mp_main(void){
	// 我们现在在高EIP，安全地切换到 kern_pgdir
	lcr3(PADDR(kern_pgdir));
	cprintf("SMP: CPU %d starting\n", cpunum());

	lapic_init();
	env_init_percpu();
	trap_init_percpu();
	xchg(&thiscpu->cpu_status, CPU_STARTED);		// 告诉 boot_aps() 我们启动了

	// 现在我们已经完成了一些基本设置，调用sched_yield()来开始在这个CPU上运行进程
	// 但请确保每次只有一个CPU可以进入调度程序
	
	for(;;);
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
	cprintf("kernel panic on CPU %d at %s:%d: ", cpunum(), file, line);
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
	// 内核发生警告的 文件+行数
	cprintf("kernel warning on CPU %d at %s:%d: ", cpunum(), file, line);
	vcprintf(fmt, ap);
	cprintf("\n");
	va_end(ap);
}
