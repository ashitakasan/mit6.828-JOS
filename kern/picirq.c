#include <inc/assert.h>
#include <inc/trap.h>

#include <kern/picirq.h>

// 当前IRQ掩码，初始IRQ掩码使能中断2（从8259A）
uint16_t irq_mask_8259A = 0xFFFF & ~(1 << IRQ_SLAVE);
static bool didinit;

/*
  初始化 8259A 中断控制器
 */
void pic_init(void){
	didinit = 1;

	// 屏蔽所有中断
	outb(IO_PIC1+1, 0xFF);
	outb(IO_PIC2+1, 0xFF);

	// 设置 主8259A
	// ICW1: 
	// 	g: 0 = 边缘触发, 1 = 电平触发
	// 	h: 0 = 级联PIC, 1 = 主机
	// 	i: 0 = 无 ICW4, 1 = 需要 ICW4
	outb(IO_PIC1, 0x11);

	// ICW2: 向量偏移
	outb(IO_PIC1+1, IRQ_OFFSET);

	// ICW3: 连接到从PIC（主PIC）的IR线的位屏蔽
	// 		从机连接到主机（从机PIC）的3位的AIRline
	outb(IO_PIC1+1, 1<<IRQ_SLAVE);

	// ICW4:
	// 	n: 1 = 特殊完全嵌套模式
	// 	b: 1 = 缓冲模式
	// 	m: 0 = 从 PIC， 1 = 主 PIC
	// 	a: 1 = 自动EOI模式
	// 	p: 0 = MCS-80/85 模式， 1 = intel x86 模式
	outb(IO_PIC1+1, 0x3);

	// 设置 从8259A
	outb(IO_PIC2, 0x11);				// ICW1
	outb(IO_PIC2+1, IRQ_OFFSET + 8);	// ICW2
	outb(IO_PIC2+1, IRQ_SLAVE);		// ICW3
	// NB自动EOI模式不会在从设备上工作
	outb(IO_PIC2+1, 0x01);			// ICW4

	// OCW3:
	// 	ef: 0x = NOP, 10 = 清除特定的掩码, 11 = 设置特定掩码
	// 	p : 0 = 没有轮询, 1 = 轮询模式
	// 	rs: 0x = NOP, 10 = 读 IRR, 11 = 读 ISR
	outb(IO_PIC1, 0x68);				// 清除特定的掩码
	outb(IO_PIC1, 0x0a);				// 默认情况下读取 IRR

	outb(IO_PIC2, 0x68);				// OCW3
	outb(IO_PIC2, 0x0a);				// OCW3

	if (irq_mask_8259A != 0xFFFF)
		irq_setmask_8259A(irq_mask_8259A);
}

void irq_setmask_8259A(uint16_t mask){
	int i;
	irq_mask_8259A = mask;
	if(!didinit)
		return;

	outb(IO_PIC1+1, (char)mask);
	outb(IO_PIC2+1, (char)(mask >> 8));
	cprintf("enabled interrupts:");
	for (i = 0; i < 16; i++)
		if (~mask & (1<<i))
			cprintf(" %d", i);
	cprintf("\n");
}
