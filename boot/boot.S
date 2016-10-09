#include <inc/mmu.h>

# 启动CPU，切换到32位模式，跳转到 C 代码
# BIOS从硬盘的第一扇区开始加载这段代码 到 物理地址为 0x7c00 的地方，开始执行程序，CS=0，IP=7c00

# set指令类似于 宏定义
.set PORT_MODE_CSEG,	0x8 				# 内核代码段选择器
.set PORT_MODE_DSEG,	0x10				# 内核数据段选择器
.set CR0_PE_ON,		0x1 				# 保护模式使能标志

# 定义全局名字 start
.globl start
start:
	.code16							# CPU 刚启动为16位模式
	cli								# 关中断
	cld								# 清方向标志

	# 设置几个重要的寄存器
	xorw	%ax, %ax					# ax清零
	movw	%ax, %ds					# dx数据段清零
	movw	%ax, %es					# es附加段清零
	movw	%ax, %ss					# ss堆栈段清零

# 打开 A20 地址线，为了兼容早期的PC，第20跟地址线在实模式下不能使用
# 所以超过 1MB 的地址，默认就会返回到地址0，重新从0循环计数
# 下边的代码打开 A20 地址线
seta20.1:
	inb		$0x64, %al				# 从0x64端口读入一个字节的数据到 al 中
	testb	$0x2, %al				# 测试 al 的第2位是否为0
	jnz		seta20.1					# 如果al的第2位为0，就不执行该指令，否则就循环检查

	movb	$0xd1, %al				# 将0xd1, 写入到al中
	outb	%al, $0x64				# 将al的数据写入到端口 0x64 中

seta20.2:
	inb		$0x64, %al				# 从0x64端口读入一个字节的数据到 al 中
	testb	$0x2, %al				# test可以当做and指令，但它不影响操作数
	jnz		seta20.2					# 如果al的第2位为0，就不执行该指令，否则就循环检查

	movb	$0xdf, %al				# 将0xdf写入到al中
	outb	%al, $0x60				# 将al的数据写入到端口 0x60 中

	# 将实模式转换为保护模式，使用GDT引导和段翻译，使得虚拟地址对应的物理地址保持不变
	# 有效储存器映射在切换时保持不变

	# 将全局描述符表加载到描述符表寄存器
	lgdt	gdtdesc
	# cr0中第0位为1表示处于保护模式，为0表示处于实模式
	movl	%cr0, %eax				# 把控制寄存器cr0加载到eax中
	orl		$CR0_PE_ON, %eax			# 将eax中的第0位 置1
	movl	%eax, %cr0 				# 将eax中的值装入cr0中

	# 跳转到32位模式中的下一条指令，将处理请切换为32位工作模式
	# 下边的指令会将 $PORT_MODE_CSEG 加载到CS中，
	# CS对应的高速缓存储存器会加载代码段描述符，同样将 $protcseg 加载到 IP 中
	ljmp	$PORT_MODE_CSEG, $protcseg

	.code32							# 32位模式汇编
protcseg:
	# 设置保护模式下的数据寄存器，将数据段选择器装入到 ax 中
	movw	$PORT_MODE_DSEG, %ax
	# 将 ax 装入到其他的数据寄存器中，
	# 在装入的同时，数据段描述符会自动的加入到这些段寄存器对应的高速缓存寄存器中
	movw	%ax, %ds 				# 数据段
	movw	%ax, %es 				# 附加段
	movw	%ax, %fs					# FS
	movw	%ax, %gs 				# GS
	movw	%ax, %ss 				# 堆栈段

	# 设置栈指针，并且调用 C 函数
	movl	$start, %esp
	call	bootmain

# 如果bootmain返回的话，就一直循环
spin:
	jmp spin

# 引导GDT
.p2align		2						# 强制4字节对齐
gdt:
	SEG_NULL
	SEG(STA_X|STA_R, 0x0, 0xffffffff)	# 代码段描述符
	SEG(STA_W, 0x0, 0xffffffff)		# 数据段描述符

# 全局描述符表对应的描述符
gdtdesc:
	.word	0x17						# sizeof(gdt) - 1
	.long	gdt 						# gdt地址
