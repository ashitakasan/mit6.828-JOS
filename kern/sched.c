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

	// 实现简单的轮询调度
	// 从这个 CPU 上次运行的环境env 开始，以循环方式从 envs 中搜索 ENV_RUNNABLE 的环境env，
	// 切换到第一个找到的这样的环境
	// 
	// 如果没有 envs 是可运行的，但以前在这个CPU上运行的环境仍然是 ENV_RUNNING，可以选择该环境
	// 
	// 千万不要 选择当前在另一个CPU上运行的环境 (env_status == ENV_RUNNING)
	// 如果没有可运行的环境，只需直接到下面的代码停止cpu
	// LAB 4

	int start = curenv ? (curenv - envs) : NENV - 1;
	int i;

	for(i = 1; i < NENV; i++){
		idle = &envs[(start + i) % NENV];
		if(idle->env_status == ENV_RUNNABLE)
			env_run(idle);
	}

	for(i = 0; i < NENV; i++){
		idle = &envs[(start + i) % NENV];
		if(idle->env_status == ENV_RUNNING && idle->env_cpunum == cpunum())
			env_run(idle);
	}
	
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
