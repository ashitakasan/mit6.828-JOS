#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/monitor.h>

struct Env *envs = NULL;				// 全部的运行环境
struct Env *curenv = NULL;			// 当前运行环境
static struct Env *env_free_list;	// 空闲环境列表； env->env_link

#define ENVGENSHIFT		12			// 大于等于 LOGNENV


// 全局描述符表
// 为内核模式和用户模式设置具有单独段的全局描述符表（GDT），
// 为内核模式和用户模式设置具有单独段的全局描述符表（GDT），
// 我们不使用它们的任何内存映射功能，但是我们需要它们来切换特权级别；
// 
// 除了DPL，内核和用户段是相同的，要装载SS寄存器，CPL必须等于DPL，
// 因此，我们必须为用户和内核复制 段；
// 特别的，在gdt的定义中使用的SEG宏的最后一个参数指定该描述符的描述符特权级别（DPL），内核 0，用户 3

struct Segdesc gdt[] = {
	// 0, 未使用（总是故障 - 用于捕获NULL远指针）
	SEG_NULL,
	// 0x8 内核代码段
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),
	// 0x10 内核数据段
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),


}



