// 本地 APIC 管理内部 (非I/O) 中断

#include <inc/types.h>
#include <inc/memlayout.h>
#include <inc/trap.h>
#include <inc/mmu.h>
#include <inc/stdio.h>
#include <inc/x86.h>
#include <kern/pmap.h>
#include <kern/cpu.h>

// 本地APIC寄存器，除以4，用作uint32_t []索引
#define ID			(0x0020/4)   // ID
#define VER			(0x0030/4)   // Version
#define TPR			(0x0080/4)   // 任务优先级
#define EOI			(0x00B0/4)   // EOI
#define SVR			(0x00F0/4)   // 伪中断向量
#define ENABLE		0x00000100   // 单元启用
#define ESR			(0x0280/4)   // 错误状态
#define ICRLO		(0x0300/4)   // 中断命令
#define INIT			0x00000500   // INIT/RESET
#define STARTUP		0x00000600   // Startup IPI
#define DELIVS		0x00001000   // Delivery status
#define ASSERT		0x00004000   // Assert interrupt (vs deassert)
#define DEASSERT		0x00000000
#define LEVEL		0x00008000   // 电平触发
#define BCAST		0x00080000   // 发送到所有APIC，包括自我.
#define OTHERS		0x000C0000   // 发送到所有APIC，不包括自我.
#define BUSY			0x00001000
#define FIXED		0x00000000
#define ICRHI		(0x0310/4)   // 中断命令 [63:32]
#define TIMER		(0x0320/4)   // Local Vector Table 0 (TIMER)
#define X1			0x0000000B   // divide counts by 1
#define PERIODIC		0x00020000   // Periodic
#define PCINT		(0x0340/4)   // 性能计数器 LVT
#define LINT0		(0x0350/4)   // Local Vector Table 1 (LINT0)
#define LINT1		(0x0360/4)   // Local Vector Table 2 (LINT1)
#define ERROR		(0x0370/4)   // Local Vector Table 3 (ERROR)
#define MASKED		0x00010000   // 中断屏蔽
#define TICR			(0x0380/4)   // 定时器初始计数
#define TCCR			(0x0390/4)   // 定时器当前计数
#define TDCR			(0x03E0/4)   // 定时器分频配置

physaddr_t lapicaddr;			// 在 mpconfig.c 中初始化
volatile uint32_t *lapic;

static void lapicw(int index, int value){
	lapic[index] = value;
	lapic[ID];					// 等待写入完成
}

/*
  初始化 LAPIC，每个 CPU 都有一个 APIC 来接收中断
 */
void lapic_init(void){
	if(!lapicaddr)
		return;

	// lapices是 LAPIC 的 4K MMIO区域的物理地址，将其映射到虚拟内存中，以便我们可以访问它
	lapic = mmio_map_region(lapicaddr, 4096);

	// 定时器从 lapic[TICR] 总线频率重复递减计数，然后发出中断
	// 如果我们更多地关心精确计时，则使用外部时间源来校准TICR
	lapicw(TDCR, X1);
	lapicw(TIMER, PERIODIC | (IRQ_OFFSET + IRQ_TIMER));
	lapicw(TICR, 10000000);

	// 让BSP的INT0使能，以便它可以从8259A芯片获得中断
	// 根据英特尔MP规范，BIOS应该以虚线模式初始化BSP的本地APIC，其中8259A的INTR虚拟连接到BSP LATIN0
	// 在这种模式下，我们不需要对IOAPIC进行编程。
	if(thiscpu != bootcpu)
		lapicw(LINT0, MASKED);

	// 在所有CPU上禁用 NMI（LINT1）
	lapicw(LINT1, MASKED);

	// 在提供该中断条目的机器上禁用性能计数器溢出中断
	if(((lapic[VER] >> 16) & 0xFF) >= 4)
		lapicw(PCINT, MASKED);

	// 清除错误状态寄存器
	lapicw(ESR, 0);
	lapicw(ESR, 0);

	// 确认任何未完成的中断
	lapicw(EOI, 0);

	// 发送Init Level Deassert
	lapicw(ICRHI, 0);
	lapicw(ICRLO, BCAST | INIT | LEVEL);
	while(lapic[ICRLO] & DELIVS)
		;

	// 在 APIC 上启用中断（但不在处理器上）
	lapicw(TPR, 0);
}

int cpunum(void){
	if(lapic)
		return lapic[ID] >> 24;
	return 0;
}

// 确认中断
void lapic_eoi(void){
	if(lapic)
		lapicw(EOI, 0);
}

// 旋转给定的微秒数，在真实的硬件上想要动态调整
static void microdelay(int us){

}

#define IO_RTC 0x70

/*
  在 addr 处启动其他处理器运行的条目代码
 */
void lapic_startap(uint8_t apicid, uint32_t addr){
	int i;
	uint16_t *wrv;

	// BSP必须在[通用启动算法]之前将CMOS关断代码初始化为 0AH，
	// 并将热复位向量（DWORD基于40:67）指向AP启动代码，
	outb(IO_RTC, 0xF);							// 偏移0xF是关闭代码
	outb(IO_RTC+1, 0x0A);
	wrv = (uint16_t *)KADDR((0x40 << 4 | 0x67));	// 热复位向量
	wrv[0] = 0;
	wrv[1] = addr >> 4;

	// 通用启动算法
	// 发送 INIT（电平触发）中断以复位其他CPU
	lapicw(ICRHI, apicid << 24);
	lapicw(ICRLO, INIT | LEVEL | ASSERT);
	microdelay(200);
	lapicw(ICRLO, INIT | LEVEL);
	microdelay(100);

	// 发送启动 IPI（两次）来输入代码
	// 普通硬件由于 INIT 而处于暂停状态时，应该只接受 STARTUP
	// 所以第二个应该忽略，但它是官方Intel算法的一部分
	for (i = 0; i < 2; i++) {
		lapicw(ICRHI, apicid << 24);
		lapicw(ICRLO, STARTUP | (addr >> 12));
		microdelay(200);
	}
}

void lapic_ipi(int vector){
	lapicw(ICRLO, OTHERS | FIXED | vector);
	while (lapic[ICRLO] & DELIVS)
		;
}
