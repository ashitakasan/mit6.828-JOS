#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>
#include <kern/sched.h>
#include <kern/kclock.h>
#include <kern/picirq.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

static struct Taskstate ts;

/*
  对于调试，打印陷阱帧可以区分打印保存的陷阱帧和打印当前陷阱帧，并在后一种情况下打印一些附加信息
 */
static struct Trapframe *last_tf;

/*
  中断描述符表，必须在运行时构建，因为移位函数地址无法在重定位记录中表示
 */
struct Gatedesc idt[256] = { { 0 } };
struct Pseudodesc idt_pd = {
	sizeof(idt) - 1, (uint32_t) idt
};

extern uint32_t trap_handlers[];

static const char *trapname(int trapno){
	static const char * const excnames[] = {
		"Divide error",
		"Debug",
		"Non-Maskable Interrupt",
		"Breakpoint",
		"Overflow",
		"BOUND Range Exceeded",
		"Invalid Opcode",
		"Device Not Available",
		"Double Fault",
		"Coprocessor Segment Overrun",
		"Invalid TSS",
		"Segment Not Present",
		"Stack Fault",
		"General Protection",
		"Page Fault",
		"(unknown trap)",
		"x87 FPU Floating-Point Error",
		"Alignment Check",
		"Machine-Check",
		"SIMD Floating-Point Exception"
	};

	if(trapno < ARRAY_SIZE(excnames))
		return excnames[trapno];
	if(trapno == T_SYSCALL)
		return "System call";
	if(trapno == IRQ_OFFSET)
		return "Clock Interrupt";
	if(trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
		return "Hardware Interrupt";
	return "(unknown trap)";
}

void trap_init(void){
	extern struct Segdesc gdt[];
	// LAB 3

	int i=0;
	for(; i < 256; i++){
		SETGATE(idt[i], 0, GD_KT, trap_handlers[i], 0);
	}

	SETGATE(idt[T_BRKPT], 0, GD_KT, trap_handlers[T_BRKPT], 3);
	SETGATE(idt[T_SYSCALL], 0, GD_KT, trap_handlers[T_SYSCALL], 3);

	trap_init_percpu();
}

/*
  初始化并加载每个CPU的 TSS和IDT
 */
void trap_init_percpu(void){
	// 示例代码在此为CPU 0设置任务状态段 (TSS) 和TSS描述符
	// 但是如果我们在其他CPU上运行是不正确的，因为每个CPU都有自己的内核堆栈
	// 修复代码，使其适用于所有CPU
	// 
	// 提示：
	// 	宏 thiscpu 总是指当前 CPU 的 struct CpuInfo
	// 	当前CPU的ID由 cpunum() 或 thiscpu->cpu_id 给出
	// 	使用 thiscpu->cpu_ts 作为当前CPU的 TSS，而不是全局 ts 变量
	// 	对 CPU i 的 TSS 描述符使用 gdt[(GD_TSS0 >> 3) + i]
	// 	将每个CPU内核堆栈映射到 mem_init_mp()
	// 
	// ltr在TSS选择器中设置一个“忙”标志，所以如果你不小心在多个CPU上加载了同一个TSS，你会得到一个三重故障
	// 如果将某个CPU的TSS设置为错误，则在尝试从该CPU上的用户空间返回之前，可能不会出现故障
	// LAB 4
	
	uint8_t cpuid = thiscpu->cpu_id;
	thiscpu->cpu_ts.ts_esp0 = KSTACKTOP - cpuid * (KSTKSIZE + KSTKGAP);
	thiscpu->cpu_ts.ts_ss0 = GD_KD;
	thiscpu->cpu_ts.ts_iomb = sizeof(struct Taskstate);

	gdt[(GD_TSS0 >> 3) + cpuid] = 
			SEG16(STS_T32A, (uint32_t)(&(thiscpu->cpu_ts)), sizeof(struct Taskstate) - 1, 0);
	gdt[(GD_TSS0 >> 3) + cpuid].sd_s = 0;

	ltr(GD_TSS0 + cpuid * sizeof(struct Segdesc));

	lidt(&idt_pd);

	// 设置一个TSS，以便当我们陷入内核时，我们得到正确的堆栈
	// ts.ts_esp0 = KSTACKTOP;
	// ts.ts_ss0 = GD_KD;
	// ts.ts_iomb = sizeof(struct Taskstate);

	// 初始化gdt的 TSS槽
	// gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t)(&ts), sizeof(struct Taskstate) - 1, 0);
	// gdt[GD_TSS0 >> 3].sd_s = 0;

	// 加载TSS选择器，像其他段选择器一样，底部三位是特殊的; 这里置零
	// ltr(GD_TSS0);

	// 加载 IDT
	// lidt(&idt_pd);
}

void print_trapframe(struct Trapframe *tf){
	cprintf("TRAP frame at %p from CPU %d\n", tf, cpunum());
	print_regs(&tf->tf_regs);
	cprintf("  es   0x----%04x\n", tf->tf_es);
	cprintf("  ds   0x----%04x\n", tf->tf_ds);
	cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trapname(tf->tf_trapno));
	// If this trap was a page fault that just happened
	// (so %cr2 is meaningful), print the faulting linear address.
	if (tf == last_tf && tf->tf_trapno == T_PGFLT)
		cprintf("  cr2  0x%08x\n", rcr2());
	cprintf("  err  0x%08x", tf->tf_err);
	// For page faults, print decoded fault error code:
	// U/K=fault occurred in user/kernel mode
	// W/R=a write/read caused the fault
	// PR=a protection violation caused the fault (NP=page not present).
	if (tf->tf_trapno == T_PGFLT)
		cprintf(" [%s, %s, %s]\n",
			tf->tf_err & 4 ? "user" : "kernel",
			tf->tf_err & 2 ? "write" : "read",
			tf->tf_err & 1 ? "protection" : "not-present");
	else
		cprintf("\n");
	cprintf("  eip  0x%08x\n", tf->tf_eip);
	cprintf("  cs   0x----%04x\n", tf->tf_cs);
	cprintf("  flag 0x%08x\n", tf->tf_eflags);
	if ((tf->tf_cs & 3) != 0) {
		cprintf("  esp  0x%08x\n", tf->tf_esp);
		cprintf("  ss   0x----%04x\n", tf->tf_ss);
	}
}

void print_regs(struct PushRegs *regs){
	cprintf("  edi  0x%08x\n", regs->reg_edi);
	cprintf("  esi  0x%08x\n", regs->reg_esi);
	cprintf("  ebp  0x%08x\n", regs->reg_ebp);
	cprintf("  oesp 0x%08x\n", regs->reg_oesp);
	cprintf("  ebx  0x%08x\n", regs->reg_ebx);
	cprintf("  edx  0x%08x\n", regs->reg_edx);
	cprintf("  ecx  0x%08x\n", regs->reg_ecx);
	cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static void trap_dispatch(struct Trapframe *tf){
	// LAB 3: 处理处理器异常

	switch(tf->tf_trapno){
		case T_PGFLT:
			page_fault_handler(tf);
			return;
		case T_BRKPT:
		case T_DEBUG:
			print_trapframe(tf);
			monitor(tf);
			return;
		case T_SYSCALL:
			tf->tf_regs.reg_eax = syscall(tf->tf_regs.reg_eax,
										tf->tf_regs.reg_edx,
										tf->tf_regs.reg_ecx,
										tf->tf_regs.reg_ebx,
										tf->tf_regs.reg_edi,
										tf->tf_regs.reg_esi);
			return;
	}

	// 处理伪中断硬件有时会由于IRQ线路上的噪声或其他原因而引起这些中断
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_SPURIOUS){
		cprintf("Spurious interrupt on irq 7\n");
		print_trapframe(tf);
		return;
	}

	// 处理定时器中断；不要忘记在调用调度程序之前使用 lapic_eoi() 确认中断
	// LAB 4

	if(tf->tf_trapno == IRQ_OFFSET + IRQ_TIMER){
		lapic_eoi();
		sched_yield();
	}

	// 处理键盘和串行中断
	// LAB 5

	if(tf->tf_trapno == IRQ_OFFSET + IRQ_KBD){
		kbd_intr();
		return;
	}
	if(tf->tf_trapno == IRQ_OFFSET + IRQ_SERIAL){
		serial_intr();
		return;
	}

	// 意外陷阱：用户进程或内核有错误
	print_trapframe(tf);
	if(tf->tf_cs == GD_KT)
		panic("unhandled trap in kernel");
	else{
		env_destroy(curenv);
		return;
	}
}

void trap(struct Trapframe *tf){
	// 环境可能已经设置DF，并且GCC的一些版本依赖于DF是清楚的
	asm volatile("cld" ::: "cc");

	// 如果有其他CPU调用panic()，停止这个CPU
	extern char *panicstr;
	if(panicstr)
		asm volatile("hlt");

	// 如果我们在 sched_yield() 上停止了，重新获取大内核锁
	if(xchg(&thiscpu->cpu_status, CPU_STARTED) == CPU_HALTED)
		lock_kernel();

	// 检查中断是否禁用。 如果这个断言失败，不要试图通过在中断路径中插入“cli”来解决它
	assert(!(read_eflags() & FL_IF));

	// cprintf("Incoming TRAP frame at %p\n", tf);

	if((tf->tf_cs & 3) == 3){
		// 从用户态进入陷阱，在做任何严重的内核工作之前获取大内核锁
		// LAB 4
		assert(curenv);

		lock_kernel();

		// 如果当前的环境是一个僵尸，则进行垃圾收集
		if(curenv->env_status == ENV_DYING){
			env_free(curenv);
			curenv = NULL;
			sched_yield();
		}

		// 将陷阱帧（其当前在堆栈上）复制到 curenv-> env_tf 中，以便运行环境将在陷阱点重新启动
		curenv->env_tf = *tf;
		// 从这里应该忽略堆栈上的陷阱帧
		tf = &curenv->env_tf;
	}

	// 记录tf是最后一个真正的陷阱帧，因此print_trapframe可以打印一些附加信息
	last_tf = tf;

	// 基于发生什么类型的陷阱进行调度
	trap_dispatch(tf);

	// 返回到当前环境，应该正在运行，恢复发生中断的进程
	// assert(curenv && curenv->env_status == ENV_RUNNING);
	// env_run(curenv);
	
	// 如果我们执行到这里，那么没有其他环境被安排，所以我们应该返回到当前环境
	if(curenv && curenv->env_status == ENV_RUNNING)
		env_run(curenv);
	else
		sched_yield();
}

void page_fault_handler(struct Trapframe * tf){
	uint32_t fault_va;

	// 读取处理器的CR2寄存器以查找故障地址
	fault_va = rcr2();

	// LAB 3: 处理内核模式页面错误

	if(tf->tf_cs == GD_KT){
		print_trapframe(tf);
		panic("kernel page fault va %08x", fault_va);
	}

	// 我们已经处理了内核模式异常，所以如果执行到这里，页面错误发生在用户模式；
	// 如果存在环境的页面错误 upcall 则调用，
	// 在用户异常堆栈 (低于 UXSTACKTOP) 上设置页错误堆栈帧，然后分支到 curenv->env_pgfault_upcall；
	// 页面错误 upcall 可能导致另一个页面错误，在这种情况下，
	// 我们向上递归到页面错误，在用户异常堆栈顶部推送另一个页面错误堆栈帧；
	// 
	// 对于从页错误返回的代码 (lib/pfentry.S)，在陷阱时间堆栈顶部
	// 	有一个临时空格的字是很方便的；它允许我们更容易恢复 eip/esp；
	// 在非递归的情况下，我们不必担心这一点，因为常规用户堆栈的顶部是空闲的；
	// 在递归的情况下，这意味着我们必须在异常堆栈的当前顶部和新的堆栈帧之间留下一个额外的字，
	// 因为异常堆栈是 trap-time 堆栈；
	// 
	// 如果没有页面错误 upcall，或者环境没有为其异常堆栈分配页面，
	// 或者不能写入它，或者异常堆栈溢出，需要破坏导致故障的环境；
	// 注意，grade 脚本假定您将首先检查页面故障 upcall，如果没有则打印如下的 '用户故障va' 消息；
	// 剩余的三个检查可以组合成单个测试
	// 
	// user_mem_assert() 和 env_run() 比较有用
	// 要更改用户环境运行，请修改 curenv-> env_tf ('tf'变量指向'curenv->env_tf')
	// LAB 4

	if(curenv->env_pgfault_upcall == NULL)
		goto user_fault;

	user_mem_assert(curenv, (void *)(UXSTACKTOP - 4), 4, PTE_U);

	uintptr_t new_stack;
	struct UTrapframe *utf;

	if(tf->tf_esp >= UXSTACKTOP - PGSIZE && tf->tf_esp <= UXSTACKTOP - 1)
		new_stack = tf->tf_esp - 4;
	else
		new_stack = UXSTACKTOP;

	// check space for utf, in recursive case
	if(new_stack - sizeof(struct UTrapframe) < UXSTACKTOP - PGSIZE)
		goto user_fault;

	utf = (struct UTrapframe *)(new_stack - sizeof(struct UTrapframe));
	utf->utf_fault_va = fault_va;
	utf->utf_err = tf->tf_err;
	utf->utf_regs = tf->tf_regs;
	utf->utf_eip = tf->tf_eip;
	utf->utf_eflags = tf->tf_eflags;
	utf->utf_esp = tf->tf_esp;

	tf->tf_esp = (uintptr_t)utf;
	tf->tf_eip = (uintptr_t)curenv->env_pgfault_upcall;

	env_run(curenv);

	// 我们已经处理了内核模式异常，所以如果我们到达这里，页面错误发生在用户模式
	// 销毁导致故障的环境

user_fault:
	cprintf("[%08x] user fault va %08x ip %08x\n", curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
