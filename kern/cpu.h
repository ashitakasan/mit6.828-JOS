#ifndef JOS_INC_CPU_H
#define JOS_INC_CPU_H

#include <inc/types.h>
#include <inc/memlayout.h>
#include <inc/mmu.h>
#include <inc/env.h>

// 最大CPU数
#define NCPU 8

// struct Cpu中的状态值
enum {
	CPU_UNUSED = 0,
	CPU_STARTED,
	CPU_HALTED,
};

// 每个 CPU 状态
struct CpuInfo {
	uint8_t cpu_id;					// cpus 索引
	volatile unsigned cpu_status;	// CPU 状态
	struct Env *cpu_env;				// 当前运行的环境
	struct Taskstate cpu_ts;			// 用于x86查找堆栈中断
};

// 在 mpconfig.c 中初始化
extern struct CpuInfo cpus[NCPU];
extern int ncpu;						// 系统中的CPU总数
extern struct CpuInfo *bootcpu;		// 引导处理器（BSP）
extern physaddr_t lapicaddr;			// 本地APIC的物理MMIO地址

// 每个 CPU 的内核栈
extern unsigned char percpu_kstacks[NCPU][KSTKSIZE];

int cpunum(void);
#define thiscpu (&cpus[cpunum()])

void mp_init(void);
void lapic_init(void);
void lapic_startap(uint8_t apicid, uint32_t addr);
void lapic_eoi(void);
void lapic_ipi(int vector);

#endif
