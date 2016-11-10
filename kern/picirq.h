#ifndef JOS_KERN_PICIRQ_H
#define JOS_KERN_PICIRQ_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#define MAX_IRQS 16		// IRQs 数目

// 两个8259A可编程中断控制器的 I/O 地址
#define IO_PIC1 0x20		// 主
#define IO_PIC2 0xA0		// 从

#define IRQ_SLAVE 2		// IRQ，从器件连接到主器件


#ifndef __ASSEMBLER__

#include <inc/types.h>
#include <inc/x86.h>

extern uint16_t irq_mask_8259A;
void pic_init(void);
void irq_setmask_8259A(uint16_t mask);

#endif // !__ASSEMBLER__

#endif // !JOS_KERN_PICIRQ_H
