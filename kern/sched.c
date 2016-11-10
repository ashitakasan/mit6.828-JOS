#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

/*
  选择一个用户环境以运行和运行它
 */
void sched_yield(void){
	struct Env *idle;

	// LAB 4
	
	sched_halt();
}

/*
  当 CPU 无事可做时，暂停这个 CPU，等待定时器中断唤醒，此函数不会返回
 */
void sched_halt(void){
	int i;

	// 为了调试和测试目的，如果系统中没有可运行的环境，则进入内核监视器
	for(i = 0; i < NENV; i++){
		if((envs[i].env_status == ENV_RUNNABLE ||
			envs[i].env_status == ENV_RUNNING ||
			envs[i].env_status == ENV_DYING))
			break;
	}
	if(i == NENV){
		cprintf("No runnable environments in the system!\n");
		while(1)
			monitor(NULL);
	}

	// 标记此CPU上没有运行任何环境
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// 标记这个CPU处于HALT状态，这样当定时器中断进来时，我们知道我们应该重新获取 内核锁
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// 释放大内核锁，就像我们“离开”内核一样
	unlock_kernel();

	// 复位堆栈指针，使能中断，然后停止
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}
