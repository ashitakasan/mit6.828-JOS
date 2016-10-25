#ifndef JOS_INC_MMU_H
#define JOS_INC_MMU_H

/*
 该文件定义了x86内存管理，包括分页、分段相关的数据结构和常量，
 %cr0, %cr4, %eflags 寄存器和陷阱
 */

/*
	第一部分，分页数据结构和常量
 */

/*
 线性地址 'la' 由三部分组成
	+--------10------+-------10-------+---------12----------+
	| Page Directory |   Page Table   | Offset within Page  |
	|      Index     |      Index     |                     |
	+----------------+----------------+---------------------+
	 \--- PDX(la) --/ \--- PTX(la) --/ \---- PGOFF(la) ----/
	 \---------- PGNUM(la) ----------/
 上图解析了 PDX, PTX, PGOFF, and PGNUM 几个宏定义；
 构造一个线性地址的方法是：PGADDR(PDX(la), PTX(la), PGOFF(la)).
*/

// 地址的页编号字段
#define	PGNUM(la)	(((uintptr_t)(la)) >> PTXSHIFT)

// 页目录索引
#define	PDX(la)		((((uintptr_t)(la)) >> PDXSHIFT) & 0x3FF)

// 页表的索引
#define	PTX(la)		((((uintptr_t)(la)) >> PTXSHIFT) & 0x3FF)

// 页偏移
#define	PGOFF(la)	(((uintptr_t)(la)) & 0xFFF)

// 从索引和偏移量构建线性地址
#define	PGADDR(d, t, o)	((void *)((d) << PDXSHIFT | (t) << PTXSHIFT | (o)))

// 页目录和页表常量
#define NPDENTRIES	1024		// 每页目录页目录项
#define NPTENTRIES	1024		// 每个页表页表项

#define PGSIZE		4096		// 每个分页映射字节数
#define PGSHIFT		12		// log2(PGSIZE)

#define PTSIZE		(PGSIZE * NPTENTRIES) // 整个页表映射的字节数
#define PTSHIFT		22		// log2(PTSIZE)

#define PTXSHIFT		12		// 线性地址 PTX 偏移量
#define PDXSHIFT		22		// 线性地址 PDX 偏移量

// 页表/目录项标志
#define PTE_P		0x001	// 当前
#define PTE_W		0x002	// 可写
#define PTE_U		0x004	// 用户
#define PTE_PWT		0x008	// 直写式
#define PTE_PCD		0x010	// 缓存禁用
#define PTE_A		0x020	// 访问过
#define PTE_D		0x040	// 脏页
#define PTE_PS		0x080	// 页大小
#define PTE_G		0x100	// 全局页

// PTE_AVAIL位不用于内核或硬件中断，因此，用户进程允许任意设置它们
#define	PTE_AVAIL	0xE00	// 可用于软件的使用

// 标志 PTE_SYSCALL 可用于系统调用
#define	PTE_SYSCALL	(PTE_AVAIL | PTE_P | PTE_W | PTE_U)

// 在页表或页目录的入口地址
#define	PTE_ADDR(pte)	((physaddr_t) (pte) & ~0xFFF)

// 控制寄存器标志
#define CR0_PE		0x00000001	// 保护启用
#define CR0_MP		0x00000002	// 监视器协处理器
#define CR0_EM		0x00000004	// 仿真
#define CR0_TS		0x00000008	// 任务切换
#define CR0_ET		0x00000010	// 扩展类型
#define CR0_NE		0x00000020	// 数字错误
#define CR0_WP		0x00010000	// 写保护
#define CR0_AM		0x00040000	// 对齐掩码
#define CR0_NW		0x20000000	// 非直写式
#define CR0_CD		0x40000000	// 缓存禁用
#define CR0_PG		0x80000000	// 分页

#define CR4_PCE		0x00000100	// 性能计数器启用
#define CR4_MCE		0x00000040	// 机器检查启用
#define CR4_PSE		0x00000010	// 页面大小扩展
#define CR4_DE		0x00000008	// 调试扩展
#define CR4_TSD		0x00000004	// 禁用时间戳
#define CR4_PVI		0x00000002	// 保护模式虚拟中断
#define CR4_VME		0x00000001	// V86模式扩展

// Eflags 寄存器
#define FL_CF		0x00000001	// 进位标志
#define FL_PF		0x00000004	// 奇偶标志
#define FL_AF		0x00000010	// 辅助进位标志
#define FL_ZF		0x00000040	// 零标志
#define FL_SF		0x00000080	// 符号标志
#define FL_TF		0x00000100	// 陷阱标志
#define FL_IF		0x00000200	// 中断标志
#define FL_DF		0x00000400	// 方向标志
#define FL_OF		0x00000800	// 溢出标志
#define FL_IOPL_MASK	0x00003000	// I/O 权限级别位掩码
#define FL_IOPL_0	0x00000000	//   IOPL == 0
#define FL_IOPL_1	0x00001000	//   IOPL == 1
#define FL_IOPL_2	0x00002000	//   IOPL == 2
#define FL_IOPL_3	0x00003000	//   IOPL == 3
#define FL_NT		0x00004000	// 嵌套任务
#define FL_RF		0x00010000	// 恢复标志
#define FL_VM		0x00020000	// 虚拟8086模式
#define FL_AC		0x00040000	// 对齐检查
#define FL_VIF		0x00080000	// 虚拟中断标志
#define FL_VIP		0x00100000	// 虚拟中断挂起
#define FL_ID		0x00200000	// ID 标志

// 页错误代码
#define FEC_PR		0x1	// 保护冲突引起的页面错误
#define FEC_WR		0x2	// 写入引起的页面错误
#define FEC_U		0x4	// 用户模式发生的页面错误


/*
	第二部分：分段数据结构和常量
 */

#ifdef __ASSEMBLER__

/*
 * Macros to build GDT entries in assembly.
 */
#define SEG_NULL						\
	.word 0, 0;						\
	.byte 0, 0, 0, 0
#define SEG(type,base,lim)					\
	.word (((lim) >> 12) & 0xffff), ((base) & 0xffff);	\
	.byte (((base) >> 16) & 0xff), (0x90 | (type)),		\
		(0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#else	// not __ASSEMBLER__

#include <inc/types.h>

// 段描述符
struct Segdesc {
	unsigned sd_lim_15_0 : 16;  // 段限制低位
	unsigned sd_base_15_0 : 16; // 段基址的低位
	unsigned sd_base_23_16 : 8; // 段基址的中间位
	unsigned sd_type : 4;       // 段类型 (see STS_ constants)
	unsigned sd_s : 1;          // 0 = 系统, 1 = 应用
	unsigned sd_dpl : 2;        // 描述符权限级别
	unsigned sd_p : 1;          // Present
	unsigned sd_lim_19_16 : 4;  // 段限制高位
	unsigned sd_avl : 1;        // 未使用（可用于软件使用）
	unsigned sd_rsv1 : 1;       // 保留的
	unsigned sd_db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
	unsigned sd_g : 1;          // Granularity: limit scaled by 4K when set
	unsigned sd_base_31_24 : 8; // 段基址的高位
};

// 空段
#define SEG_NULL		(struct Segdesc){ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
// 段是可读的，但使用时出现故障
#define SEG_FAULT	(struct Segdesc){ 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 0 }
// 普通段
#define SEG(type, base, lim, dpl) (struct Segdesc)				\
	{ ((lim) >> 12) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,	\
		type, 1, dpl, 1, (unsigned) (lim) >> 28, 0, 0, 1, 1,			\
		(unsigned) (base) >> 24 }
#define SEG16(type, base, lim, dpl) (struct Segdesc)				\
	{ (lim) & 0xffff, (base) & 0xffff, ((base) >> 16) & 0xff,		\
		type, 1, dpl, 1, (unsigned) (lim) >> 16, 0, 0, 1, 0,			\
		(unsigned) (base) >> 24 }

#endif /* !__ASSEMBLER__ */

// 应用程序段类型位
#define STA_X		0x8	    // 可执行段
#define STA_E		0x4	    // 展开向下（非可执行段）
#define STA_C		0x4	    // 相容代码段（仅可执行文件）
#define STA_W		0x2	    // 可写（非可执行的段）
#define STA_R		0x2	    // 可读（可执行段）
#define STA_A		0x1	    // 已访问

// 系统段类型位
#define STS_T16A		0x1	    // 可用的16位TSS
#define STS_LDT		0x2	    // 局部描述符表
#define STS_T16B		0x3	    // 忙的16位TSS
#define STS_CG16		0x4	    // 16位调用门
#define STS_TG		0x5	    // 任务门/ Coum变速器
#define STS_IG16		0x6	    // 16位中断门
#define STS_TG16		0x7	    // 16位陷阱门
#define STS_T32A		0x9	    // 可用的32位TSS
#define STS_T32B		0xB	    // 忙的32位TSS
#define STS_CG32		0xC	    // 32位调用门
#define STS_IG32		0xE	    // 32位中断门
#define STS_TG32		0xF	    // 32位陷阱门


/*
 *	Part 3.  陷阱.
 */

#ifndef __ASSEMBLER__

// 任务状态段的格式（如奔腾架构书中描述）
struct Taskstate {
	uint32_t ts_link;	// 旧的 ts 选择器
	uintptr_t ts_esp0;	// 栈指针和段选择器
	uint16_t ts_ss0;		// 增加特许级后
	uint16_t ts_padding1;
	uintptr_t ts_esp1;
	uint16_t ts_ss1;
	uint16_t ts_padding2;
	uintptr_t ts_esp2;
	uint16_t ts_ss2;
	uint16_t ts_padding3;
	physaddr_t ts_cr3;	// 页目录基地址
	uintptr_t ts_eip;	// 上一任务切换的保存状态
	uint32_t ts_eflags;
	uint32_t ts_eax;		// 更多的保存状态 (寄存器)
	uint32_t ts_ecx;
	uint32_t ts_edx;
	uint32_t ts_ebx;
	uintptr_t ts_esp;
	uintptr_t ts_ebp;
	uint32_t ts_esi;
	uint32_t ts_edi;
	uint16_t ts_es;		// 更多的保存状态 (段选择器)
	uint16_t ts_padding4;
	uint16_t ts_cs;
	uint16_t ts_padding5;
	uint16_t ts_ss;
	uint16_t ts_padding6;
	uint16_t ts_ds;
	uint16_t ts_padding7;
	uint16_t ts_fs;
	uint16_t ts_padding8;
	uint16_t ts_gs;
	uint16_t ts_padding9;
	uint16_t ts_ldt;
	uint16_t ts_padding10;
	uint16_t ts_t;		// 任务切换的陷阱
	uint16_t ts_iomb;	// I/O 映射基地址
};

// 中断和陷阱步态描述
struct Gatedesc {
	unsigned gd_off_15_0 : 16;   // low 16 bits of offset in segment
	unsigned gd_sel : 16;        // 段选择器
	unsigned gd_args : 5;        // # args, 0 for interrupt/trap gates
	unsigned gd_rsv1 : 3;        // reserved(should be zero I guess)
	unsigned gd_type : 4;        // type(STS_{TG,IG32,TG32})
	unsigned gd_s : 1;           // must be 0 (system)
	unsigned gd_dpl : 2;         // descriptor(meaning new) privilege level
	unsigned gd_p : 1;           // Present
	unsigned gd_off_31_16 : 16;  // high bits of offset in segment
};

// 设置一个正常中断/陷阱门描述符
// - istrap: 1 表示陷阱 (= 异常) 门, 0 表示中断门
    //   see section 9.6.1.3 of the i386 reference: "The difference between
    //   an interrupt gate and a trap gate is in the effect on IF (the
    //   interrupt-enable flag). An interrupt that vectors through an
    //   interrupt gate resets IF, thereby preventing other interrupts from
    //   interfering with the current interrupt handler. A subsequent IRET
    //   instruction restores IF to the value in the EFLAGS image on the
    //   stack. An interrupt through a trap gate does not change IF."
// - sel: 代码段选择 中断/陷阱处理程序
// - off: 代码段偏移量 中断/陷阱处理程序
// - dpl: 描述符权限级别
//	   对于软件所需的权限级别来调用此中断/陷阱门使用明确的int指令
#define SETGATE(gate, istrap, sel, off, dpl)			\
{													\
	(gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;		\
	(gate).gd_sel = (sel);							\
	(gate).gd_args = 0;								\
	(gate).gd_rsv1 = 0;								\
	(gate).gd_type = (istrap) ? STS_TG32 : STS_IG32;	\
	(gate).gd_s = 0;									\
	(gate).gd_dpl = (dpl);							\
	(gate).gd_p = 1;									\
	(gate).gd_off_31_16 = (uint32_t) (off) >> 16;		\
}

// 建立呼叫门描述符
#define SETCALLGATE(gate, sel, off, dpl)				\
{													\
	(gate).gd_off_15_0 = (uint32_t) (off) & 0xffff;		\
	(gate).gd_sel = (sel);							\
	(gate).gd_args = 0;								\
	(gate).gd_rsv1 = 0;								\
	(gate).gd_type = STS_CG32;						\
	(gate).gd_s = 0;									\
	(gate).gd_dpl = (dpl);							\
	(gate).gd_p = 1;									\
	(gate).gd_off_31_16 = (uint32_t) (off) >> 16;		\
}

// LGDT，LLDT和LIFT指令的伪描述符
struct Pseudodesc {
	uint16_t pd_lim;			// Limit
	uint32_t pd_base;		// Base address
} __attribute__ ((packed));

#endif /* !__ASSEMBLER__ */

#endif /* !JOS_INC_MMU_H */
