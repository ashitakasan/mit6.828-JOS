#include <inc/mmu.h>
#include <inc/x86.h>
#include <inc/assert.h>

#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/env.h>
#include <kern/syscall.h>

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
	// 设置一个TSS，以便当我们陷入内核时，我们得到正确的堆栈
	ts.ts_esp0 = KSTACKTOP;
	ts.ts_ss0 = GD_KD;

	// 初始化gdt的 TSS槽
	gdt[GD_TSS0 >> 3] = SEG16(STS_T32A, (uint32_t)(&ts), sizeof(struct Taskstate) - 1, 0);
	gdt[GD_TSS0 >> 3].sd_s = 0;

	// 加载TSS选择器，像其他段选择器一样，底部三位是特殊的; 这里置零
	ltr(GD_TSS0);

	// 加载 IDT
	lidt(&idt_pd);
}

void print_trapframe(struct Trapframe *tf){
	cprintf("TRAP frame at %p\n", tf);
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

	// 检查中断是否禁用。 如果这个断言失败，不要试图通过在中断路径中插入“cli”来解决它
	assert(!(read_eflags() & FL_IF));

	cprintf("Incoming TRAP frame at %p\n", tf);

	if((tf->tf_cs & 3) == 3){
		assert(curenv);

		// 将陷阱帧（其当前在堆栈上）复制到“curenv-> env_tf”中，以便运行环境将在陷阱点重新启动
		curenv->env_tf = *tf;
		// 从这里应该忽略堆栈上的陷阱帧
		tf = &curenv->env_tf;
	}

	// 记录tf是最后一个真正的陷阱帧，因此print_trapframe可以打印一些附加信息
	last_tf = tf;

	// 基于发生什么类型的陷阱进行调度
	trap_dispatch(tf);

	// 返回到当前环境，应该正在运行
	assert(curenv && curenv->env_status == ENV_RUNNING);
	env_run(curenv);
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

	// 我们已经处理了内核模式异常，所以如果我们到达这里，页面错误发生在用户模式
	// 销毁导致故障的环境
	cprintf("[%08x] user fault va %08x ip %08x\n", curenv->env_id, fault_va, tf->tf_eip);
	print_trapframe(tf);
	env_destroy(curenv);
}
