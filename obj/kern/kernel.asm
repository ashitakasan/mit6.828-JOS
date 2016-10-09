
obj/kern/kernel：     文件格式 elf32-i386


Disassembly of section .text:

f0100000 <_start+0xeffffff4>:
.globl	_start
_start = RELOC(entry)

.globl entry
entry:
	movw	$0x1234, 0x472		# 热启动，kernel第一条指令
f0100000:	02 b0 ad 1b 00 00    	add    0x1bad(%eax),%dh
f0100006:	00 00                	add    %al,(%eax)
f0100008:	fe 4f 52             	decb   0x52(%edi)
f010000b:	e4 66                	in     $0x66,%al

f010000c <entry>:
f010000c:	66 c7 05 72 04 00 00 	movw   $0x1234,0x472
f0100013:	34 12 
	# 但是kernel的C代码应该在 内存的高地址空间执行，因此需要通过地址转换，
	# 将虚拟的高地址空间 [3840MB, 3844MB]映射到物理的低地址空间 [0, 4MB]。
	# 在启用 mem_init 之前，这里 4MB地址空间足够用了。

	# 将物理地址页表 entry_pgdir 到 寄存器cr3中
	movl	$(RELOC(entry_pgdir)), %eax
f0100015:	b8 00 00 11 00       	mov    $0x110000,%eax
	movl	%eax, %cr3
f010001a:	0f 22 d8             	mov    %eax,%cr3
	# 启用分页
	movl	%cr0, %eax
f010001d:	0f 20 c0             	mov    %cr0,%eax
	orl		$(CR0_PE|CR0_PG|CR0_WP), %eax
f0100020:	0d 01 00 01 80       	or     $0x80010001,%eax
	movl	%eax, %cr0
f0100025:	0f 22 c0             	mov    %eax,%cr0

	# 现在分页已经启用，但仍然运行在低地址的指令指针寄存器（EIP）
	# 在执行 C代码前，跳转到高地址空间
	mov 		$relocated, %eax
f0100028:	b8 2f 00 10 f0       	mov    $0xf010002f,%eax
	jmp		*%eax
f010002d:	ff e0                	jmp    *%eax

f010002f <relocated>:

relocated:
	# 清除帧指针寄存器（EBP）
	# 因此，一旦我们进入调试C代码，调用栈将被正确终止
	movl	$0x0, %ebp
f010002f:	bd 00 00 00 00       	mov    $0x0,%ebp

	# 设置堆栈指针
	movl	$(bootstacktop), %esp
f0100034:	bc 00 00 11 f0       	mov    $0xf0110000,%esp

	# 开始调用 C代码，初始化OS
	call	i386_init
f0100039:	e8 56 00 00 00       	call   f0100094 <i386_init>

f010003e <spin>:

# 自旋，这里不应该被调用
spin:	jmp spin
f010003e:	eb fe                	jmp    f010003e <spin>

f0100040 <test_backtrace>:

#include <kern/monitor.h>
#include <kern/console.h>

// 测试栈回溯功能
void test_backtrace(int x){
f0100040:	55                   	push   %ebp
f0100041:	89 e5                	mov    %esp,%ebp
f0100043:	53                   	push   %ebx
f0100044:	83 ec 0c             	sub    $0xc,%esp
f0100047:	8b 5d 08             	mov    0x8(%ebp),%ebx
	cprintf("entering test_backtrace %d\n", x);
f010004a:	53                   	push   %ebx
f010004b:	68 00 19 10 f0       	push   $0xf0101900
f0100050:	e8 f5 08 00 00       	call   f010094a <cprintf>
	if(x > 0)
f0100055:	83 c4 10             	add    $0x10,%esp
f0100058:	85 db                	test   %ebx,%ebx
f010005a:	7e 11                	jle    f010006d <test_backtrace+0x2d>
		test_backtrace(x-1);
f010005c:	83 ec 0c             	sub    $0xc,%esp
f010005f:	8d 43 ff             	lea    -0x1(%ebx),%eax
f0100062:	50                   	push   %eax
f0100063:	e8 d8 ff ff ff       	call   f0100040 <test_backtrace>
f0100068:	83 c4 10             	add    $0x10,%esp
f010006b:	eb 11                	jmp    f010007e <test_backtrace+0x3e>
	else
		mon_backtrace(0, 0, 0);
f010006d:	83 ec 04             	sub    $0x4,%esp
f0100070:	6a 00                	push   $0x0
f0100072:	6a 00                	push   $0x0
f0100074:	6a 00                	push   $0x0
f0100076:	e8 bb 06 00 00       	call   f0100736 <mon_backtrace>
f010007b:	83 c4 10             	add    $0x10,%esp
	cprintf("leaving test_backtrace %d\n", x);
f010007e:	83 ec 08             	sub    $0x8,%esp
f0100081:	53                   	push   %ebx
f0100082:	68 1c 19 10 f0       	push   $0xf010191c
f0100087:	e8 be 08 00 00       	call   f010094a <cprintf>
f010008c:	83 c4 10             	add    $0x10,%esp
}
f010008f:	8b 5d fc             	mov    -0x4(%ebp),%ebx
f0100092:	c9                   	leave  
f0100093:	c3                   	ret    

f0100094 <i386_init>:

/*
 内核初始化
 */
void i386_init(void){
f0100094:	55                   	push   %ebp
f0100095:	89 e5                	mov    %esp,%ebp
f0100097:	83 ec 0c             	sub    $0xc,%esp
	extern char edata[], end[];

	// 执行其他任务之前，完成ELF加载过程；
	// 清除我们的程序的未初始化的全局数据（BSS）部分；
	// 这将确保所有静态/全局变量开始于零
	memset(edata, 0, end - edata);
f010009a:	b8 84 29 11 f0       	mov    $0xf0112984,%eax
f010009f:	2d 00 23 11 f0       	sub    $0xf0112300,%eax
f01000a4:	50                   	push   %eax
f01000a5:	6a 00                	push   $0x0
f01000a7:	68 00 23 11 f0       	push   $0xf0112300
f01000ac:	e8 82 13 00 00       	call   f0101433 <memset>

	// 初始化终端，在此之前不能调用 cprintf
	cons_init();
f01000b1:	e8 84 04 00 00       	call   f010053a <cons_init>

	cprintf("6.828 decimal is %o octal!\n", 6828);
f01000b6:	83 c4 08             	add    $0x8,%esp
f01000b9:	68 ac 1a 00 00       	push   $0x1aac
f01000be:	68 37 19 10 f0       	push   $0xf0101937
f01000c3:	e8 82 08 00 00       	call   f010094a <cprintf>

	// 测试栈回溯功能
	test_backtrace(5);
f01000c8:	c7 04 24 05 00 00 00 	movl   $0x5,(%esp)
f01000cf:	e8 6c ff ff ff       	call   f0100040 <test_backtrace>
f01000d4:	83 c4 10             	add    $0x10,%esp

	// 陷入内核监控
	while(1)
		monitor(NULL);
f01000d7:	83 ec 0c             	sub    $0xc,%esp
f01000da:	6a 00                	push   $0x0
f01000dc:	e8 eb 06 00 00       	call   f01007cc <monitor>
f01000e1:	83 c4 10             	add    $0x10,%esp
f01000e4:	eb f1                	jmp    f01000d7 <i386_init+0x43>

f01000e6 <_panic>:
const char *panicstr;

/*
 发生无法解决的致命错误时 _panic 被调用，它打印"panic: mesg"，然后进入内核监控
 */
void _panic(const char *file, int line, const char *fmt, ...){
f01000e6:	55                   	push   %ebp
f01000e7:	89 e5                	mov    %esp,%ebp
f01000e9:	56                   	push   %esi
f01000ea:	53                   	push   %ebx
f01000eb:	8b 75 10             	mov    0x10(%ebp),%esi
	va_list ap;

	if(panicstr)
f01000ee:	83 3d 80 29 11 f0 00 	cmpl   $0x0,0xf0112980
f01000f5:	75 37                	jne    f010012e <_panic+0x48>
		goto dead;
	panicstr = fmt;
f01000f7:	89 35 80 29 11 f0    	mov    %esi,0xf0112980

	// 要特别确保设备处于合理状态
	asm volatile("cli; cld");
f01000fd:	fa                   	cli    
f01000fe:	fc                   	cld    

	va_start(ap, fmt);
f01000ff:	8d 5d 14             	lea    0x14(%ebp),%ebx
	// 内核发生错误的 文件+行数
	cprintf("kernel panic at %s:%d: ", file, line);
f0100102:	83 ec 04             	sub    $0x4,%esp
f0100105:	ff 75 0c             	pushl  0xc(%ebp)
f0100108:	ff 75 08             	pushl  0x8(%ebp)
f010010b:	68 53 19 10 f0       	push   $0xf0101953
f0100110:	e8 35 08 00 00       	call   f010094a <cprintf>
	vcprintf(fmt, ap);		// 打印可变参数
f0100115:	83 c4 08             	add    $0x8,%esp
f0100118:	53                   	push   %ebx
f0100119:	56                   	push   %esi
f010011a:	e8 05 08 00 00       	call   f0100924 <vcprintf>
	cprintf("\n");
f010011f:	c7 04 24 8f 19 10 f0 	movl   $0xf010198f,(%esp)
f0100126:	e8 1f 08 00 00       	call   f010094a <cprintf>
	va_end(ap);
f010012b:	83 c4 10             	add    $0x10,%esp

dead:
	while(1)					// 陷入内核监控
		monitor(NULL);
f010012e:	83 ec 0c             	sub    $0xc,%esp
f0100131:	6a 00                	push   $0x0
f0100133:	e8 94 06 00 00       	call   f01007cc <monitor>
f0100138:	83 c4 10             	add    $0x10,%esp
f010013b:	eb f1                	jmp    f010012e <_panic+0x48>

f010013d <_warn>:
}

/*
 内核发生警告
 */
void _warn(const char *file, int line, const char *fmt, ...){
f010013d:	55                   	push   %ebp
f010013e:	89 e5                	mov    %esp,%ebp
f0100140:	53                   	push   %ebx
f0100141:	83 ec 08             	sub    $0x8,%esp
	va_list ap;

	va_start(ap, fmt);
f0100144:	8d 5d 14             	lea    0x14(%ebp),%ebx
	// 内核发生错误的 文件+行数
	cprintf("kernel warning at %s:%d: ", file, line);
f0100147:	ff 75 0c             	pushl  0xc(%ebp)
f010014a:	ff 75 08             	pushl  0x8(%ebp)
f010014d:	68 6b 19 10 f0       	push   $0xf010196b
f0100152:	e8 f3 07 00 00       	call   f010094a <cprintf>
	vcprintf(fmt, ap);
f0100157:	83 c4 08             	add    $0x8,%esp
f010015a:	53                   	push   %ebx
f010015b:	ff 75 10             	pushl  0x10(%ebp)
f010015e:	e8 c1 07 00 00       	call   f0100924 <vcprintf>
	cprintf("\n");
f0100163:	c7 04 24 8f 19 10 f0 	movl   $0xf010198f,(%esp)
f010016a:	e8 db 07 00 00       	call   f010094a <cprintf>
	va_end(ap);
f010016f:	83 c4 10             	add    $0x10,%esp
}
f0100172:	8b 5d fc             	mov    -0x4(%ebp),%ebx
f0100175:	c9                   	leave  
f0100176:	c3                   	ret    

f0100177 <serial_proc_data>:


/*
 从串口获取数据
 */
static int serial_proc_data(void){
f0100177:	55                   	push   %ebp
f0100178:	89 e5                	mov    %esp,%ebp

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f010017a:	ba fd 03 00 00       	mov    $0x3fd,%edx
f010017f:	ec                   	in     (%dx),%al
	if(!(inb(COM1 + COM_LSR) & COM_LSR_DATA))
f0100180:	a8 01                	test   $0x1,%al
f0100182:	74 08                	je     f010018c <serial_proc_data+0x15>
f0100184:	b2 f8                	mov    $0xf8,%dl
f0100186:	ec                   	in     (%dx),%al
		return -1;
	return inb(COM1 + COM_RX);
f0100187:	0f b6 c0             	movzbl %al,%eax
f010018a:	eb 05                	jmp    f0100191 <serial_proc_data+0x1a>
/*
 从串口获取数据
 */
static int serial_proc_data(void){
	if(!(inb(COM1 + COM_LSR) & COM_LSR_DATA))
		return -1;
f010018c:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
	return inb(COM1 + COM_RX);
}
f0100191:	5d                   	pop    %ebp
f0100192:	c3                   	ret    

f0100193 <cons_intr>:


/*
 调用设备中断子程序，输入字符送入循环控制台的输入缓冲区。
 */
static void cons_intr(int (*proc)(void)){
f0100193:	55                   	push   %ebp
f0100194:	89 e5                	mov    %esp,%ebp
f0100196:	53                   	push   %ebx
f0100197:	83 ec 04             	sub    $0x4,%esp
f010019a:	89 c3                	mov    %eax,%ebx
	int c;

	while((c = (*proc)()) != -1){
f010019c:	eb 2a                	jmp    f01001c8 <cons_intr+0x35>
		if(c == 0)
f010019e:	85 d2                	test   %edx,%edx
f01001a0:	74 26                	je     f01001c8 <cons_intr+0x35>
			continue;
		cons.buf[cons.wpos++] = c;
f01001a2:	a1 44 25 11 f0       	mov    0xf0112544,%eax
f01001a7:	8d 48 01             	lea    0x1(%eax),%ecx
f01001aa:	89 0d 44 25 11 f0    	mov    %ecx,0xf0112544
f01001b0:	88 90 40 23 11 f0    	mov    %dl,-0xfeedcc0(%eax)
		if(cons.wpos == CONSBUFSIZE)
f01001b6:	81 f9 00 02 00 00    	cmp    $0x200,%ecx
f01001bc:	75 0a                	jne    f01001c8 <cons_intr+0x35>
			cons.wpos = 0;
f01001be:	c7 05 44 25 11 f0 00 	movl   $0x0,0xf0112544
f01001c5:	00 00 00 
 调用设备中断子程序，输入字符送入循环控制台的输入缓冲区。
 */
static void cons_intr(int (*proc)(void)){
	int c;

	while((c = (*proc)()) != -1){
f01001c8:	ff d3                	call   *%ebx
f01001ca:	89 c2                	mov    %eax,%edx
f01001cc:	83 f8 ff             	cmp    $0xffffffff,%eax
f01001cf:	75 cd                	jne    f010019e <cons_intr+0xb>
			continue;
		cons.buf[cons.wpos++] = c;
		if(cons.wpos == CONSBUFSIZE)
			cons.wpos = 0;
	}
}
f01001d1:	83 c4 04             	add    $0x4,%esp
f01001d4:	5b                   	pop    %ebx
f01001d5:	5d                   	pop    %ebp
f01001d6:	c3                   	ret    

f01001d7 <kbd_proc_data>:
f01001d7:	ba 64 00 00 00       	mov    $0x64,%edx
f01001dc:	ec                   	in     (%dx),%al
	int c;
	uint8_t stat, data;
	static uint32_t shift;

	stat = inb(KBSTATP);
	if((stat & KBS_DIB) == 0)
f01001dd:	a8 01                	test   $0x1,%al
f01001df:	0f 84 f0 00 00 00    	je     f01002d5 <kbd_proc_data+0xfe>
		return -1;
	if(stat & KBS_TERR)			// 忽略鼠标
f01001e5:	a8 20                	test   $0x20,%al
f01001e7:	0f 85 ee 00 00 00    	jne    f01002db <kbd_proc_data+0x104>
f01001ed:	b2 60                	mov    $0x60,%dl
f01001ef:	ec                   	in     (%dx),%al
f01001f0:	89 c2                	mov    %eax,%edx
		return -1;

	data = inb(KBDATAP);

	if (data == 0xE0) {			// E0 表示转义字符
f01001f2:	3c e0                	cmp    $0xe0,%al
f01001f4:	75 0d                	jne    f0100203 <kbd_proc_data+0x2c>
		shift |= E0ESC;
f01001f6:	83 0d 00 23 11 f0 40 	orl    $0x40,0xf0112300
		return 0;
f01001fd:	b8 00 00 00 00       	mov    $0x0,%eax
f0100202:	c3                   	ret    
	}
	else if (data & 0x80) {		// 键释放
f0100203:	84 c0                	test   %al,%al
f0100205:	79 2f                	jns    f0100236 <kbd_proc_data+0x5f>
		data = (shift & E0ESC ? data : data & 0x7F);
f0100207:	8b 0d 00 23 11 f0    	mov    0xf0112300,%ecx
f010020d:	f6 c1 40             	test   $0x40,%cl
f0100210:	75 05                	jne    f0100217 <kbd_proc_data+0x40>
f0100212:	83 e0 7f             	and    $0x7f,%eax
f0100215:	89 c2                	mov    %eax,%edx
		shift &= ~(shiftcode[data] | E0ESC);
f0100217:	0f b6 c2             	movzbl %dl,%eax
f010021a:	0f b6 80 00 1b 10 f0 	movzbl -0xfefe500(%eax),%eax
f0100221:	83 c8 40             	or     $0x40,%eax
f0100224:	0f b6 c0             	movzbl %al,%eax
f0100227:	f7 d0                	not    %eax
f0100229:	21 c8                	and    %ecx,%eax
f010022b:	a3 00 23 11 f0       	mov    %eax,0xf0112300
		return 0;
f0100230:	b8 00 00 00 00       	mov    $0x0,%eax
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
		cprintf("Rebooting!\n");
		outb(0x92, 0x3);		// 克里斯·弗罗斯特的礼节
	}
	return c;
}
f0100235:	c3                   	ret    


/*
 从键盘获取数据。如果完成一个字符，返回它，否则为0；没有数据则返回-1
 */
static int kbd_proc_data(void){
f0100236:	55                   	push   %ebp
f0100237:	89 e5                	mov    %esp,%ebp
f0100239:	53                   	push   %ebx
f010023a:	83 ec 04             	sub    $0x4,%esp
	else if (data & 0x80) {		// 键释放
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	}
	else if (shift & E0ESC) {	// 最后一个字符是一个E0或0x80则逃逸
f010023d:	8b 0d 00 23 11 f0    	mov    0xf0112300,%ecx
f0100243:	f6 c1 40             	test   $0x40,%cl
f0100246:	74 0e                	je     f0100256 <kbd_proc_data+0x7f>
		data |= 0x80;
f0100248:	83 c8 80             	or     $0xffffff80,%eax
f010024b:	89 c2                	mov    %eax,%edx
		shift &= ~E0ESC;
f010024d:	83 e1 bf             	and    $0xffffffbf,%ecx
f0100250:	89 0d 00 23 11 f0    	mov    %ecx,0xf0112300
	}

	shift |= shiftcode[data];
f0100256:	0f b6 c2             	movzbl %dl,%eax
f0100259:	0f b6 90 00 1b 10 f0 	movzbl -0xfefe500(%eax),%edx
f0100260:	0b 15 00 23 11 f0    	or     0xf0112300,%edx
	shift ^= togglecode[data];
f0100266:	0f b6 88 00 1a 10 f0 	movzbl -0xfefe600(%eax),%ecx
f010026d:	31 ca                	xor    %ecx,%edx
f010026f:	89 15 00 23 11 f0    	mov    %edx,0xf0112300

	c = charcode[shift & (CTL | SHIFT)][data];
f0100275:	89 d1                	mov    %edx,%ecx
f0100277:	83 e1 03             	and    $0x3,%ecx
f010027a:	8b 0c 8d c0 19 10 f0 	mov    -0xfefe640(,%ecx,4),%ecx
f0100281:	0f b6 04 01          	movzbl (%ecx,%eax,1),%eax
f0100285:	0f b6 d8             	movzbl %al,%ebx
	if (shift & CAPSLOCK) {
f0100288:	f6 c2 08             	test   $0x8,%dl
f010028b:	74 1a                	je     f01002a7 <kbd_proc_data+0xd0>
		if ('a' <= c && c <= 'z')
f010028d:	89 d8                	mov    %ebx,%eax
f010028f:	8d 4b 9f             	lea    -0x61(%ebx),%ecx
f0100292:	83 f9 19             	cmp    $0x19,%ecx
f0100295:	77 05                	ja     f010029c <kbd_proc_data+0xc5>
			c += 'A' - 'a';
f0100297:	83 eb 20             	sub    $0x20,%ebx
f010029a:	eb 0b                	jmp    f01002a7 <kbd_proc_data+0xd0>
		else if ('A' <= c && c <= 'Z')
f010029c:	83 e8 41             	sub    $0x41,%eax
f010029f:	83 f8 19             	cmp    $0x19,%eax
f01002a2:	77 03                	ja     f01002a7 <kbd_proc_data+0xd0>
			c += 'a' - 'A';
f01002a4:	83 c3 20             	add    $0x20,%ebx
	}

	// 处理特殊键，Ctrl-Alt-Del: 重启
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
f01002a7:	81 fb e9 00 00 00    	cmp    $0xe9,%ebx
f01002ad:	75 32                	jne    f01002e1 <kbd_proc_data+0x10a>
f01002af:	f7 d2                	not    %edx
f01002b1:	f6 c2 06             	test   $0x6,%dl
f01002b4:	75 2b                	jne    f01002e1 <kbd_proc_data+0x10a>
		cprintf("Rebooting!\n");
f01002b6:	83 ec 0c             	sub    $0xc,%esp
f01002b9:	68 85 19 10 f0       	push   $0xf0101985
f01002be:	e8 87 06 00 00       	call   f010094a <cprintf>
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
f01002c3:	ba 92 00 00 00       	mov    $0x92,%edx
f01002c8:	b8 03 00 00 00       	mov    $0x3,%eax
f01002cd:	ee                   	out    %al,(%dx)
f01002ce:	83 c4 10             	add    $0x10,%esp
		outb(0x92, 0x3);		// 克里斯·弗罗斯特的礼节
	}
	return c;
f01002d1:	89 d8                	mov    %ebx,%eax
f01002d3:	eb 0e                	jmp    f01002e3 <kbd_proc_data+0x10c>
	uint8_t stat, data;
	static uint32_t shift;

	stat = inb(KBSTATP);
	if((stat & KBS_DIB) == 0)
		return -1;
f01002d5:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f01002da:	c3                   	ret    
	if(stat & KBS_TERR)			// 忽略鼠标
		return -1;
f01002db:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f01002e0:	c3                   	ret    
	// 处理特殊键，Ctrl-Alt-Del: 重启
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
		cprintf("Rebooting!\n");
		outb(0x92, 0x3);		// 克里斯·弗罗斯特的礼节
	}
	return c;
f01002e1:	89 d8                	mov    %ebx,%eax
}
f01002e3:	8b 5d fc             	mov    -0x4(%ebp),%ebx
f01002e6:	c9                   	leave  
f01002e7:	c3                   	ret    

f01002e8 <cons_putc>:
}

/*
 输出一个字符到终端
 */
static void cons_putc(int c){
f01002e8:	55                   	push   %ebp
f01002e9:	89 e5                	mov    %esp,%ebp
f01002eb:	57                   	push   %edi
f01002ec:	56                   	push   %esi
f01002ed:	53                   	push   %ebx
f01002ee:	83 ec 1c             	sub    $0x1c,%esp
f01002f1:	89 c7                	mov    %eax,%edi
/*
 串口输出字符
 */
static void serial_putc(int c){
	int i;
	for(i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i++)
f01002f3:	bb 00 00 00 00       	mov    $0x0,%ebx

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f01002f8:	be fd 03 00 00       	mov    $0x3fd,%esi
f01002fd:	b9 84 00 00 00       	mov    $0x84,%ecx
f0100302:	eb 09                	jmp    f010030d <cons_putc+0x25>
f0100304:	89 ca                	mov    %ecx,%edx
f0100306:	ec                   	in     (%dx),%al
f0100307:	ec                   	in     (%dx),%al
f0100308:	ec                   	in     (%dx),%al
f0100309:	ec                   	in     (%dx),%al
f010030a:	83 c3 01             	add    $0x1,%ebx
f010030d:	89 f2                	mov    %esi,%edx
f010030f:	ec                   	in     (%dx),%al
f0100310:	a8 20                	test   $0x20,%al
f0100312:	75 08                	jne    f010031c <cons_putc+0x34>
f0100314:	81 fb ff 31 00 00    	cmp    $0x31ff,%ebx
f010031a:	7e e8                	jle    f0100304 <cons_putc+0x1c>
f010031c:	89 f8                	mov    %edi,%eax
f010031e:	88 45 e7             	mov    %al,-0x19(%ebp)
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
f0100321:	ba f8 03 00 00       	mov    $0x3f8,%edx
f0100326:	89 f8                	mov    %edi,%eax
f0100328:	ee                   	out    %al,(%dx)
 并口输出一个字符
 */
static void lpt_putc(int c){
	int i;

	for (i = 0; !(inb(0x378+1) & 0x80) && i < 12800; i++)
f0100329:	bb 00 00 00 00       	mov    $0x0,%ebx

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f010032e:	be 79 03 00 00       	mov    $0x379,%esi
f0100333:	b9 84 00 00 00       	mov    $0x84,%ecx
f0100338:	eb 09                	jmp    f0100343 <cons_putc+0x5b>
f010033a:	89 ca                	mov    %ecx,%edx
f010033c:	ec                   	in     (%dx),%al
f010033d:	ec                   	in     (%dx),%al
f010033e:	ec                   	in     (%dx),%al
f010033f:	ec                   	in     (%dx),%al
f0100340:	83 c3 01             	add    $0x1,%ebx
f0100343:	89 f2                	mov    %esi,%edx
f0100345:	ec                   	in     (%dx),%al
f0100346:	84 c0                	test   %al,%al
f0100348:	78 08                	js     f0100352 <cons_putc+0x6a>
f010034a:	81 fb ff 31 00 00    	cmp    $0x31ff,%ebx
f0100350:	7e e8                	jle    f010033a <cons_putc+0x52>
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
f0100352:	ba 78 03 00 00       	mov    $0x378,%edx
f0100357:	0f b6 45 e7          	movzbl -0x19(%ebp),%eax
f010035b:	ee                   	out    %al,(%dx)
f010035c:	b2 7a                	mov    $0x7a,%dl
f010035e:	b8 0d 00 00 00       	mov    $0xd,%eax
f0100363:	ee                   	out    %al,(%dx)
f0100364:	b8 08 00 00 00       	mov    $0x8,%eax
f0100369:	ee                   	out    %al,(%dx)
/*
 CGA 输出一个字符
 */
static void cga_putc(int c){
	// 如果没有属性给出，就用黑白色
	if(!(c & ~0xFF))
f010036a:	f7 c7 00 ff ff ff    	test   $0xffffff00,%edi
f0100370:	75 06                	jne    f0100378 <cons_putc+0x90>
		c |= 0x0700;
f0100372:	81 cf 00 07 00 00    	or     $0x700,%edi

	switch(c & 0xFF){
f0100378:	89 f8                	mov    %edi,%eax
f010037a:	0f b6 c0             	movzbl %al,%eax
f010037d:	83 f8 09             	cmp    $0x9,%eax
f0100380:	74 74                	je     f01003f6 <cons_putc+0x10e>
f0100382:	83 f8 09             	cmp    $0x9,%eax
f0100385:	7f 0a                	jg     f0100391 <cons_putc+0xa9>
f0100387:	83 f8 08             	cmp    $0x8,%eax
f010038a:	74 14                	je     f01003a0 <cons_putc+0xb8>
f010038c:	e9 8f 00 00 00       	jmp    f0100420 <cons_putc+0x138>
f0100391:	83 f8 0a             	cmp    $0xa,%eax
f0100394:	74 3a                	je     f01003d0 <cons_putc+0xe8>
f0100396:	83 f8 0d             	cmp    $0xd,%eax
f0100399:	74 3d                	je     f01003d8 <cons_putc+0xf0>
f010039b:	e9 80 00 00 00       	jmp    f0100420 <cons_putc+0x138>
		case '\b':				// 退格键
			if(crt_pos > 0){
f01003a0:	0f b7 05 48 25 11 f0 	movzwl 0xf0112548,%eax
f01003a7:	66 85 c0             	test   %ax,%ax
f01003aa:	0f 84 dc 00 00 00    	je     f010048c <cons_putc+0x1a4>
				crt_pos--;
f01003b0:	83 e8 01             	sub    $0x1,%eax
f01003b3:	66 a3 48 25 11 f0    	mov    %ax,0xf0112548
				crt_buf[crt_pos] = (c & ~0xFF) | ' ';
f01003b9:	0f b7 c0             	movzwl %ax,%eax
f01003bc:	66 81 e7 00 ff       	and    $0xff00,%di
f01003c1:	83 cf 20             	or     $0x20,%edi
f01003c4:	8b 15 4c 25 11 f0    	mov    0xf011254c,%edx
f01003ca:	66 89 3c 42          	mov    %di,(%edx,%eax,2)
f01003ce:	eb 6e                	jmp    f010043e <cons_putc+0x156>
			}
			break;

		case '\n':				// 换行
			crt_pos += CRT_COLS;
f01003d0:	66 83 05 48 25 11 f0 	addw   $0x50,0xf0112548
f01003d7:	50 

		case '\r':
			crt_pos -= (crt_pos % CRT_COLS);
f01003d8:	0f b7 05 48 25 11 f0 	movzwl 0xf0112548,%eax
f01003df:	69 c0 cd cc 00 00    	imul   $0xcccd,%eax,%eax
f01003e5:	c1 e8 16             	shr    $0x16,%eax
f01003e8:	8d 04 80             	lea    (%eax,%eax,4),%eax
f01003eb:	c1 e0 04             	shl    $0x4,%eax
f01003ee:	66 a3 48 25 11 f0    	mov    %ax,0xf0112548
f01003f4:	eb 48                	jmp    f010043e <cons_putc+0x156>
			break;

		case '\t':
			cons_putc(' ');
f01003f6:	b8 20 00 00 00       	mov    $0x20,%eax
f01003fb:	e8 e8 fe ff ff       	call   f01002e8 <cons_putc>
			cons_putc(' ');
f0100400:	b8 20 00 00 00       	mov    $0x20,%eax
f0100405:	e8 de fe ff ff       	call   f01002e8 <cons_putc>
			cons_putc(' ');
f010040a:	b8 20 00 00 00       	mov    $0x20,%eax
f010040f:	e8 d4 fe ff ff       	call   f01002e8 <cons_putc>
			cons_putc(' ');
f0100414:	b8 20 00 00 00       	mov    $0x20,%eax
f0100419:	e8 ca fe ff ff       	call   f01002e8 <cons_putc>
f010041e:	eb 1e                	jmp    f010043e <cons_putc+0x156>
			break;

		default:
			crt_buf[crt_pos++] = c;
f0100420:	0f b7 05 48 25 11 f0 	movzwl 0xf0112548,%eax
f0100427:	8d 50 01             	lea    0x1(%eax),%edx
f010042a:	66 89 15 48 25 11 f0 	mov    %dx,0xf0112548
f0100431:	0f b7 c0             	movzwl %ax,%eax
f0100434:	8b 15 4c 25 11 f0    	mov    0xf011254c,%edx
f010043a:	66 89 3c 42          	mov    %di,(%edx,%eax,2)
			break;
	}

	if(crt_pos >= CRT_SIZE){
f010043e:	66 81 3d 48 25 11 f0 	cmpw   $0x7cf,0xf0112548
f0100445:	cf 07 
f0100447:	76 43                	jbe    f010048c <cons_putc+0x1a4>
		int i;
		memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
f0100449:	a1 4c 25 11 f0       	mov    0xf011254c,%eax
f010044e:	83 ec 04             	sub    $0x4,%esp
f0100451:	68 00 0f 00 00       	push   $0xf00
f0100456:	8d 90 a0 00 00 00    	lea    0xa0(%eax),%edx
f010045c:	52                   	push   %edx
f010045d:	50                   	push   %eax
f010045e:	e8 1d 10 00 00       	call   f0101480 <memmove>
		for(i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
			crt_buf[i] = 0x0700 | ' ';
f0100463:	8b 15 4c 25 11 f0    	mov    0xf011254c,%edx
f0100469:	8d 82 00 0f 00 00    	lea    0xf00(%edx),%eax
f010046f:	81 c2 a0 0f 00 00    	add    $0xfa0,%edx
f0100475:	83 c4 10             	add    $0x10,%esp
f0100478:	66 c7 00 20 07       	movw   $0x720,(%eax)
f010047d:	83 c0 02             	add    $0x2,%eax
	}

	if(crt_pos >= CRT_SIZE){
		int i;
		memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
		for(i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
f0100480:	39 d0                	cmp    %edx,%eax
f0100482:	75 f4                	jne    f0100478 <cons_putc+0x190>
			crt_buf[i] = 0x0700 | ' ';
		crt_pos -= CRT_COLS;
f0100484:	66 83 2d 48 25 11 f0 	subw   $0x50,0xf0112548
f010048b:	50 
	}

	// 移动光标
	outb(addr_6845, 14);
f010048c:	8b 0d 50 25 11 f0    	mov    0xf0112550,%ecx
f0100492:	b8 0e 00 00 00       	mov    $0xe,%eax
f0100497:	89 ca                	mov    %ecx,%edx
f0100499:	ee                   	out    %al,(%dx)
	outb(addr_6845 + 1, crt_pos >> 8);
f010049a:	0f b7 1d 48 25 11 f0 	movzwl 0xf0112548,%ebx
f01004a1:	8d 71 01             	lea    0x1(%ecx),%esi
f01004a4:	89 d8                	mov    %ebx,%eax
f01004a6:	66 c1 e8 08          	shr    $0x8,%ax
f01004aa:	89 f2                	mov    %esi,%edx
f01004ac:	ee                   	out    %al,(%dx)
f01004ad:	b8 0f 00 00 00       	mov    $0xf,%eax
f01004b2:	89 ca                	mov    %ecx,%edx
f01004b4:	ee                   	out    %al,(%dx)
f01004b5:	89 d8                	mov    %ebx,%eax
f01004b7:	89 f2                	mov    %esi,%edx
f01004b9:	ee                   	out    %al,(%dx)
 */
static void cons_putc(int c){
	serial_putc(c);				// 串口输出
	lpt_putc(c);					// 并口输出
	cga_putc(c);					// CGA输出
}
f01004ba:	8d 65 f4             	lea    -0xc(%ebp),%esp
f01004bd:	5b                   	pop    %ebx
f01004be:	5e                   	pop    %esi
f01004bf:	5f                   	pop    %edi
f01004c0:	5d                   	pop    %ebp
f01004c1:	c3                   	ret    

f01004c2 <serial_intr>:

/*
 串口中断
 */
void serial_intr(void){
	if(serial_exists)
f01004c2:	80 3d 54 25 11 f0 00 	cmpb   $0x0,0xf0112554
f01004c9:	74 11                	je     f01004dc <serial_intr+0x1a>
}

/*
 串口中断
 */
void serial_intr(void){
f01004cb:	55                   	push   %ebp
f01004cc:	89 e5                	mov    %esp,%ebp
f01004ce:	83 ec 08             	sub    $0x8,%esp
	if(serial_exists)
		cons_intr(serial_proc_data);
f01004d1:	b8 77 01 10 f0       	mov    $0xf0100177,%eax
f01004d6:	e8 b8 fc ff ff       	call   f0100193 <cons_intr>
}
f01004db:	c9                   	leave  
f01004dc:	f3 c3                	repz ret 

f01004de <kbd_intr>:
}

/*
 键盘中断
 */
void kbd_intr(void){
f01004de:	55                   	push   %ebp
f01004df:	89 e5                	mov    %esp,%ebp
f01004e1:	83 ec 08             	sub    $0x8,%esp
	cons_intr(kbd_proc_data);
f01004e4:	b8 d7 01 10 f0       	mov    $0xf01001d7,%eax
f01004e9:	e8 a5 fc ff ff       	call   f0100193 <cons_intr>
}
f01004ee:	c9                   	leave  
f01004ef:	c3                   	ret    

f01004f0 <cons_getc>:
}

/*
 返回终端的下一个输入字符，没有则返回 0
 */
int cons_getc(void){
f01004f0:	55                   	push   %ebp
f01004f1:	89 e5                	mov    %esp,%ebp
f01004f3:	83 ec 08             	sub    $0x8,%esp
	int c;

	// 轮询任何挂起输入的字符，即使中断被禁止该功能仍然可用
	serial_intr();
f01004f6:	e8 c7 ff ff ff       	call   f01004c2 <serial_intr>
	kbd_intr();
f01004fb:	e8 de ff ff ff       	call   f01004de <kbd_intr>

	// 获取输入缓冲区的下一个字符
	if(cons.rpos != cons.wpos){
f0100500:	a1 40 25 11 f0       	mov    0xf0112540,%eax
f0100505:	3b 05 44 25 11 f0    	cmp    0xf0112544,%eax
f010050b:	74 26                	je     f0100533 <cons_getc+0x43>
		c = cons.buf[cons.rpos++];
f010050d:	8d 50 01             	lea    0x1(%eax),%edx
f0100510:	89 15 40 25 11 f0    	mov    %edx,0xf0112540
f0100516:	0f b6 88 40 23 11 f0 	movzbl -0xfeedcc0(%eax),%ecx
		if(cons.rpos == CONSBUFSIZE)
			cons.rpos = 0;
		return c;
f010051d:	89 c8                	mov    %ecx,%eax
	kbd_intr();

	// 获取输入缓冲区的下一个字符
	if(cons.rpos != cons.wpos){
		c = cons.buf[cons.rpos++];
		if(cons.rpos == CONSBUFSIZE)
f010051f:	81 fa 00 02 00 00    	cmp    $0x200,%edx
f0100525:	75 11                	jne    f0100538 <cons_getc+0x48>
			cons.rpos = 0;
f0100527:	c7 05 40 25 11 f0 00 	movl   $0x0,0xf0112540
f010052e:	00 00 00 
f0100531:	eb 05                	jmp    f0100538 <cons_getc+0x48>
		return c;
	}
	return 0;
f0100533:	b8 00 00 00 00       	mov    $0x0,%eax
}
f0100538:	c9                   	leave  
f0100539:	c3                   	ret    

f010053a <cons_init>:
}

/*
 终端初始化
 */
void cons_init(void){
f010053a:	55                   	push   %ebp
f010053b:	89 e5                	mov    %esp,%ebp
f010053d:	57                   	push   %edi
f010053e:	56                   	push   %esi
f010053f:	53                   	push   %ebx
f0100540:	83 ec 0c             	sub    $0xc,%esp
	volatile uint16_t *cp;
	uint16_t was;
	unsigned pos;

	cp = (uint16_t*) (KERNBASE + CGA_BUF);
	was = *cp;
f0100543:	0f b7 15 00 80 0b f0 	movzwl 0xf00b8000,%edx
	*cp = (uint16_t) 0xA55A;
f010054a:	66 c7 05 00 80 0b f0 	movw   $0xa55a,0xf00b8000
f0100551:	5a a5 
	if (*cp != 0xA55A) {
f0100553:	0f b7 05 00 80 0b f0 	movzwl 0xf00b8000,%eax
f010055a:	66 3d 5a a5          	cmp    $0xa55a,%ax
f010055e:	74 11                	je     f0100571 <cons_init+0x37>
		cp = (uint16_t *) (KERNBASE + MONO_BUF);
		addr_6845 = MONO_BASE;
f0100560:	c7 05 50 25 11 f0 b4 	movl   $0x3b4,0xf0112550
f0100567:	03 00 00 

	cp = (uint16_t*) (KERNBASE + CGA_BUF);
	was = *cp;
	*cp = (uint16_t) 0xA55A;
	if (*cp != 0xA55A) {
		cp = (uint16_t *) (KERNBASE + MONO_BUF);
f010056a:	be 00 00 0b f0       	mov    $0xf00b0000,%esi
f010056f:	eb 16                	jmp    f0100587 <cons_init+0x4d>
		addr_6845 = MONO_BASE;
	} else {
		*cp = was;
f0100571:	66 89 15 00 80 0b f0 	mov    %dx,0xf00b8000
		addr_6845 = CGA_BASE;
f0100578:	c7 05 50 25 11 f0 d4 	movl   $0x3d4,0xf0112550
f010057f:	03 00 00 
static void cga_init(void){
	volatile uint16_t *cp;
	uint16_t was;
	unsigned pos;

	cp = (uint16_t*) (KERNBASE + CGA_BUF);
f0100582:	be 00 80 0b f0       	mov    $0xf00b8000,%esi
		*cp = was;
		addr_6845 = CGA_BASE;
	}

	/* 提取光标位置 */
	outb(addr_6845, 14);
f0100587:	8b 3d 50 25 11 f0    	mov    0xf0112550,%edi
f010058d:	b8 0e 00 00 00       	mov    $0xe,%eax
f0100592:	89 fa                	mov    %edi,%edx
f0100594:	ee                   	out    %al,(%dx)
	pos = inb(addr_6845 + 1) << 8;
f0100595:	8d 4f 01             	lea    0x1(%edi),%ecx

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f0100598:	89 ca                	mov    %ecx,%edx
f010059a:	ec                   	in     (%dx),%al
f010059b:	0f b6 c0             	movzbl %al,%eax
f010059e:	c1 e0 08             	shl    $0x8,%eax
f01005a1:	89 c3                	mov    %eax,%ebx
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
f01005a3:	b8 0f 00 00 00       	mov    $0xf,%eax
f01005a8:	89 fa                	mov    %edi,%edx
f01005aa:	ee                   	out    %al,(%dx)

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f01005ab:	89 ca                	mov    %ecx,%edx
f01005ad:	ec                   	in     (%dx),%al
	outb(addr_6845, 15);
	pos |= inb(addr_6845 + 1);

	crt_buf = (uint16_t*) cp;
f01005ae:	89 35 4c 25 11 f0    	mov    %esi,0xf011254c

	/* 提取光标位置 */
	outb(addr_6845, 14);
	pos = inb(addr_6845 + 1) << 8;
	outb(addr_6845, 15);
	pos |= inb(addr_6845 + 1);
f01005b4:	0f b6 c8             	movzbl %al,%ecx
f01005b7:	89 d8                	mov    %ebx,%eax
f01005b9:	09 c8                	or     %ecx,%eax

	crt_buf = (uint16_t*) cp;
	crt_pos = pos;
f01005bb:	66 a3 48 25 11 f0    	mov    %ax,0xf0112548
}

static inline void
outb(int port, uint8_t data)
{
	asm volatile("outb %0,%w1" : : "a" (data), "d" (port));
f01005c1:	bb fa 03 00 00       	mov    $0x3fa,%ebx
f01005c6:	b8 00 00 00 00       	mov    $0x0,%eax
f01005cb:	89 da                	mov    %ebx,%edx
f01005cd:	ee                   	out    %al,(%dx)
f01005ce:	b2 fb                	mov    $0xfb,%dl
f01005d0:	b8 80 ff ff ff       	mov    $0xffffff80,%eax
f01005d5:	ee                   	out    %al,(%dx)
f01005d6:	be f8 03 00 00       	mov    $0x3f8,%esi
f01005db:	b8 0c 00 00 00       	mov    $0xc,%eax
f01005e0:	89 f2                	mov    %esi,%edx
f01005e2:	ee                   	out    %al,(%dx)
f01005e3:	b2 f9                	mov    $0xf9,%dl
f01005e5:	b8 00 00 00 00       	mov    $0x0,%eax
f01005ea:	ee                   	out    %al,(%dx)
f01005eb:	b2 fb                	mov    $0xfb,%dl
f01005ed:	b8 03 00 00 00       	mov    $0x3,%eax
f01005f2:	ee                   	out    %al,(%dx)
f01005f3:	b2 fc                	mov    $0xfc,%dl
f01005f5:	b8 00 00 00 00       	mov    $0x0,%eax
f01005fa:	ee                   	out    %al,(%dx)
f01005fb:	b2 f9                	mov    $0xf9,%dl
f01005fd:	b8 01 00 00 00       	mov    $0x1,%eax
f0100602:	ee                   	out    %al,(%dx)

static inline uint8_t
inb(int port)
{
	uint8_t data;
	asm volatile("inb %w1,%0" : "=a" (data) : "d" (port));
f0100603:	b2 fd                	mov    $0xfd,%dl
f0100605:	ec                   	in     (%dx),%al
	// 启用RCV中断
	outb(COM1 + COM_IER, COM_IER_RDI);

	// 清除任何现有的溢出指示和中断
	// 如果COM_LSR返回值为0xFF说明串行端口不存在
	serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
f0100606:	3c ff                	cmp    $0xff,%al
f0100608:	0f 95 c1             	setne  %cl
f010060b:	88 0d 54 25 11 f0    	mov    %cl,0xf0112554
f0100611:	89 da                	mov    %ebx,%edx
f0100613:	ec                   	in     (%dx),%al
f0100614:	89 f2                	mov    %esi,%edx
f0100616:	ec                   	in     (%dx),%al
void cons_init(void){
	cga_init();					// CGA 初始化
	kbd_init();					// 键盘初始化
	serial_init();				// 串口初始化

	if(!serial_exists)
f0100617:	84 c9                	test   %cl,%cl
f0100619:	75 10                	jne    f010062b <cons_init+0xf1>
		cprintf("Serial port does not exist!\n");
f010061b:	83 ec 0c             	sub    $0xc,%esp
f010061e:	68 91 19 10 f0       	push   $0xf0101991
f0100623:	e8 22 03 00 00       	call   f010094a <cprintf>
f0100628:	83 c4 10             	add    $0x10,%esp
}
f010062b:	8d 65 f4             	lea    -0xc(%ebp),%esp
f010062e:	5b                   	pop    %ebx
f010062f:	5e                   	pop    %esi
f0100630:	5f                   	pop    %edi
f0100631:	5d                   	pop    %ebp
f0100632:	c3                   	ret    

f0100633 <cputchar>:

// 终端I/O，提供给 readline和cprintf 使用

void cputchar(int c){
f0100633:	55                   	push   %ebp
f0100634:	89 e5                	mov    %esp,%ebp
f0100636:	83 ec 08             	sub    $0x8,%esp
	cons_putc(c);
f0100639:	8b 45 08             	mov    0x8(%ebp),%eax
f010063c:	e8 a7 fc ff ff       	call   f01002e8 <cons_putc>
}
f0100641:	c9                   	leave  
f0100642:	c3                   	ret    

f0100643 <getchar>:

int getchar(void){
f0100643:	55                   	push   %ebp
f0100644:	89 e5                	mov    %esp,%ebp
f0100646:	83 ec 08             	sub    $0x8,%esp
	int c;
	while((c = cons_getc()) == 0)
f0100649:	e8 a2 fe ff ff       	call   f01004f0 <cons_getc>
f010064e:	85 c0                	test   %eax,%eax
f0100650:	74 f7                	je     f0100649 <getchar+0x6>
		;
	return c;
}
f0100652:	c9                   	leave  
f0100653:	c3                   	ret    

f0100654 <iscons>:

int iscons(int fdnum){
f0100654:	55                   	push   %ebp
f0100655:	89 e5                	mov    %esp,%ebp
	return 1;
}
f0100657:	b8 01 00 00 00       	mov    $0x1,%eax
f010065c:	5d                   	pop    %ebp
f010065d:	c3                   	ret    

f010065e <mon_help>:
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
};

/*****  基本内核监控命令的实现  *****/

int mon_help(int argc, char **argv, struct Trapframe *tf){
f010065e:	55                   	push   %ebp
f010065f:	89 e5                	mov    %esp,%ebp
f0100661:	83 ec 0c             	sub    $0xc,%esp
	int i;

	for(i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
f0100664:	68 00 1c 10 f0       	push   $0xf0101c00
f0100669:	68 1e 1c 10 f0       	push   $0xf0101c1e
f010066e:	68 23 1c 10 f0       	push   $0xf0101c23
f0100673:	e8 d2 02 00 00       	call   f010094a <cprintf>
f0100678:	83 c4 0c             	add    $0xc,%esp
f010067b:	68 b8 1c 10 f0       	push   $0xf0101cb8
f0100680:	68 2c 1c 10 f0       	push   $0xf0101c2c
f0100685:	68 23 1c 10 f0       	push   $0xf0101c23
f010068a:	e8 bb 02 00 00       	call   f010094a <cprintf>
	return 0;
}
f010068f:	b8 00 00 00 00       	mov    $0x0,%eax
f0100694:	c9                   	leave  
f0100695:	c3                   	ret    

f0100696 <mon_kerninfo>:

/*
 kerninfo 帮助命令的实现
 */
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf){
f0100696:	55                   	push   %ebp
f0100697:	89 e5                	mov    %esp,%ebp
f0100699:	83 ec 14             	sub    $0x14,%esp
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
f010069c:	68 35 1c 10 f0       	push   $0xf0101c35
f01006a1:	e8 a4 02 00 00       	call   f010094a <cprintf>
	cprintf("  _start                  %08x (phys)\n", _start);
f01006a6:	83 c4 08             	add    $0x8,%esp
f01006a9:	68 0c 00 10 00       	push   $0x10000c
f01006ae:	68 e0 1c 10 f0       	push   $0xf0101ce0
f01006b3:	e8 92 02 00 00       	call   f010094a <cprintf>
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
f01006b8:	83 c4 0c             	add    $0xc,%esp
f01006bb:	68 0c 00 10 00       	push   $0x10000c
f01006c0:	68 0c 00 10 f0       	push   $0xf010000c
f01006c5:	68 08 1d 10 f0       	push   $0xf0101d08
f01006ca:	e8 7b 02 00 00       	call   f010094a <cprintf>
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
f01006cf:	83 c4 0c             	add    $0xc,%esp
f01006d2:	68 e5 18 10 00       	push   $0x1018e5
f01006d7:	68 e5 18 10 f0       	push   $0xf01018e5
f01006dc:	68 2c 1d 10 f0       	push   $0xf0101d2c
f01006e1:	e8 64 02 00 00       	call   f010094a <cprintf>
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
f01006e6:	83 c4 0c             	add    $0xc,%esp
f01006e9:	68 00 23 11 00       	push   $0x112300
f01006ee:	68 00 23 11 f0       	push   $0xf0112300
f01006f3:	68 50 1d 10 f0       	push   $0xf0101d50
f01006f8:	e8 4d 02 00 00       	call   f010094a <cprintf>
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
f01006fd:	83 c4 0c             	add    $0xc,%esp
f0100700:	68 84 29 11 00       	push   $0x112984
f0100705:	68 84 29 11 f0       	push   $0xf0112984
f010070a:	68 74 1d 10 f0       	push   $0xf0101d74
f010070f:	e8 36 02 00 00       	call   f010094a <cprintf>
f0100714:	b8 83 2d 11 f0       	mov    $0xf0112d83,%eax
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
f0100719:	2d 0c 00 10 f0       	sub    $0xf010000c,%eax
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
f010071e:	83 c4 08             	add    $0x8,%esp
f0100721:	c1 f8 0a             	sar    $0xa,%eax
f0100724:	50                   	push   %eax
f0100725:	68 98 1d 10 f0       	push   $0xf0101d98
f010072a:	e8 1b 02 00 00       	call   f010094a <cprintf>
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}
f010072f:	b8 00 00 00 00       	mov    $0x0,%eax
f0100734:	c9                   	leave  
f0100735:	c3                   	ret    

f0100736 <mon_backtrace>:

/*
 backtrace 命令的实现，调试信息，打印全部栈帧信息
 */
int mon_backtrace(int argc, char **argv, struct Trapframe *tf){
f0100736:	55                   	push   %ebp
f0100737:	89 e5                	mov    %esp,%ebp
f0100739:	57                   	push   %edi
f010073a:	56                   	push   %esi
f010073b:	53                   	push   %ebx
f010073c:	83 ec 3c             	sub    $0x3c,%esp

static inline uint32_t
read_ebp(void)
{
	uint32_t ebp;
	asm volatile("movl %%ebp,%0" : "=r" (ebp));
f010073f:	89 ee                	mov    %ebp,%esi
	uint32_t ebp = read_ebp();
	struct Eipdebuginfo info;

	while(ebp != 0){
f0100741:	eb 78                	jmp    f01007bb <mon_backtrace+0x85>
		// eip 为调用者的返回地址，ebp当前位置的下一个 int
		uint32_t eip = *(uint32_t *)(ebp + sizeof(uint32_t));
f0100743:	8b 46 04             	mov    0x4(%esi),%eax
f0100746:	89 45 c4             	mov    %eax,-0x3c(%ebp)
		cprintf("ebp %08x eip %08x args", ebp, eip);
f0100749:	83 ec 04             	sub    $0x4,%esp
f010074c:	50                   	push   %eax
f010074d:	56                   	push   %esi
f010074e:	68 4e 1c 10 f0       	push   $0xf0101c4e
f0100753:	e8 f2 01 00 00       	call   f010094a <cprintf>
f0100758:	8d 5e 08             	lea    0x8(%esi),%ebx
f010075b:	8d 7e 1c             	lea    0x1c(%esi),%edi
f010075e:	83 c4 10             	add    $0x10,%esp
		int i = 0;
		while(i < 5){
			// 当前ebp向上两个int即为上一栈帧的esp，esp向上即为当前函数栈帧的参数
			cprintf(" %08x", *(uint32_t *)(ebp + (2 + i++) * sizeof(uint32_t)));
f0100761:	83 ec 08             	sub    $0x8,%esp
f0100764:	ff 33                	pushl  (%ebx)
f0100766:	68 65 1c 10 f0       	push   $0xf0101c65
f010076b:	e8 da 01 00 00       	call   f010094a <cprintf>
f0100770:	83 c3 04             	add    $0x4,%ebx
	while(ebp != 0){
		// eip 为调用者的返回地址，ebp当前位置的下一个 int
		uint32_t eip = *(uint32_t *)(ebp + sizeof(uint32_t));
		cprintf("ebp %08x eip %08x args", ebp, eip);
		int i = 0;
		while(i < 5){
f0100773:	83 c4 10             	add    $0x10,%esp
f0100776:	39 fb                	cmp    %edi,%ebx
f0100778:	75 e7                	jne    f0100761 <mon_backtrace+0x2b>
			// 当前ebp向上两个int即为上一栈帧的esp，esp向上即为当前函数栈帧的参数
			cprintf(" %08x", *(uint32_t *)(ebp + (2 + i++) * sizeof(uint32_t)));
		}
		cprintf("\n");
f010077a:	83 ec 0c             	sub    $0xc,%esp
f010077d:	68 8f 19 10 f0       	push   $0xf010198f
f0100782:	e8 c3 01 00 00       	call   f010094a <cprintf>

		debuginfo_eip(eip, &info);
f0100787:	83 c4 08             	add    $0x8,%esp
f010078a:	8d 45 d0             	lea    -0x30(%ebp),%eax
f010078d:	50                   	push   %eax
f010078e:	8b 7d c4             	mov    -0x3c(%ebp),%edi
f0100791:	57                   	push   %edi
f0100792:	e8 c9 02 00 00       	call   f0100a60 <debuginfo_eip>
		cprintf("\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen,
f0100797:	83 c4 08             	add    $0x8,%esp
f010079a:	89 f8                	mov    %edi,%eax
f010079c:	2b 45 e0             	sub    -0x20(%ebp),%eax
f010079f:	50                   	push   %eax
f01007a0:	ff 75 d8             	pushl  -0x28(%ebp)
f01007a3:	ff 75 dc             	pushl  -0x24(%ebp)
f01007a6:	ff 75 d4             	pushl  -0x2c(%ebp)
f01007a9:	ff 75 d0             	pushl  -0x30(%ebp)
f01007ac:	68 6b 1c 10 f0       	push   $0xf0101c6b
f01007b1:	e8 94 01 00 00       	call   f010094a <cprintf>
			info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = *(uint32_t *)(ebp);
f01007b6:	8b 36                	mov    (%esi),%esi
f01007b8:	83 c4 20             	add    $0x20,%esp
 */
int mon_backtrace(int argc, char **argv, struct Trapframe *tf){
	uint32_t ebp = read_ebp();
	struct Eipdebuginfo info;

	while(ebp != 0){
f01007bb:	85 f6                	test   %esi,%esi
f01007bd:	75 84                	jne    f0100743 <mon_backtrace+0xd>
		cprintf("\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen,
			info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = *(uint32_t *)(ebp);
	}
	return 0;
}
f01007bf:	b8 00 00 00 00       	mov    $0x0,%eax
f01007c4:	8d 65 f4             	lea    -0xc(%ebp),%esp
f01007c7:	5b                   	pop    %ebx
f01007c8:	5e                   	pop    %esi
f01007c9:	5f                   	pop    %edi
f01007ca:	5d                   	pop    %ebp
f01007cb:	c3                   	ret    

f01007cc <monitor>:
}

/*
 交互式命令实现
 */
void monitor(struct Trapframe *tf){
f01007cc:	55                   	push   %ebp
f01007cd:	89 e5                	mov    %esp,%ebp
f01007cf:	57                   	push   %edi
f01007d0:	56                   	push   %esi
f01007d1:	53                   	push   %ebx
f01007d2:	83 ec 58             	sub    $0x58,%esp
	char *buf;

	cprintf("Welcome to the MIT6.828-JOS kernel monirot!\n");
f01007d5:	68 c4 1d 10 f0       	push   $0xf0101dc4
f01007da:	e8 6b 01 00 00       	call   f010094a <cprintf>
	cprintf("Type 'help' for a list of commands.\n");
f01007df:	c7 04 24 f4 1d 10 f0 	movl   $0xf0101df4,(%esp)
f01007e6:	e8 5f 01 00 00       	call   f010094a <cprintf>
f01007eb:	83 c4 10             	add    $0x10,%esp

	while(1){
		buf = readline("K> ");
f01007ee:	83 ec 0c             	sub    $0xc,%esp
f01007f1:	68 7c 1c 10 f0       	push   $0xf0101c7c
f01007f6:	e8 e1 09 00 00       	call   f01011dc <readline>
f01007fb:	89 c3                	mov    %eax,%ebx
		if(buf != NULL){
f01007fd:	83 c4 10             	add    $0x10,%esp
f0100800:	85 c0                	test   %eax,%eax
f0100802:	74 ea                	je     f01007ee <monitor+0x22>
	char *argv[MAXARGS];
	int i;

	// 将解析命令缓冲区变成空格分隔参数
	argc = 0;
	argv[argc] = 0;				// 初始化 argv
f0100804:	c7 45 a8 00 00 00 00 	movl   $0x0,-0x58(%ebp)
	int argc;
	char *argv[MAXARGS];
	int i;

	// 将解析命令缓冲区变成空格分隔参数
	argc = 0;
f010080b:	be 00 00 00 00       	mov    $0x0,%esi
f0100810:	eb 0a                	jmp    f010081c <monitor+0x50>
	argv[argc] = 0;				// 初始化 argv
	while(1){
		while(*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
f0100812:	c6 03 00             	movb   $0x0,(%ebx)
f0100815:	89 f7                	mov    %esi,%edi
f0100817:	8d 5b 01             	lea    0x1(%ebx),%ebx
f010081a:	89 fe                	mov    %edi,%esi

	// 将解析命令缓冲区变成空格分隔参数
	argc = 0;
	argv[argc] = 0;				// 初始化 argv
	while(1){
		while(*buf && strchr(WHITESPACE, *buf))
f010081c:	0f b6 03             	movzbl (%ebx),%eax
f010081f:	84 c0                	test   %al,%al
f0100821:	74 63                	je     f0100886 <monitor+0xba>
f0100823:	83 ec 08             	sub    $0x8,%esp
f0100826:	0f be c0             	movsbl %al,%eax
f0100829:	50                   	push   %eax
f010082a:	68 80 1c 10 f0       	push   $0xf0101c80
f010082f:	e8 c2 0b 00 00       	call   f01013f6 <strchr>
f0100834:	83 c4 10             	add    $0x10,%esp
f0100837:	85 c0                	test   %eax,%eax
f0100839:	75 d7                	jne    f0100812 <monitor+0x46>
			*buf++ = 0;
		if(*buf == 0)
f010083b:	80 3b 00             	cmpb   $0x0,(%ebx)
f010083e:	74 46                	je     f0100886 <monitor+0xba>
			break;

		if(argc == MAXARGS - 1){
f0100840:	83 fe 0f             	cmp    $0xf,%esi
f0100843:	75 14                	jne    f0100859 <monitor+0x8d>
			cprintf("Too many arguments (max %d)\n", MAXARGS);
f0100845:	83 ec 08             	sub    $0x8,%esp
f0100848:	6a 10                	push   $0x10
f010084a:	68 85 1c 10 f0       	push   $0xf0101c85
f010084f:	e8 f6 00 00 00       	call   f010094a <cprintf>
f0100854:	83 c4 10             	add    $0x10,%esp
f0100857:	eb 95                	jmp    f01007ee <monitor+0x22>
			return 0;
		}
		argv[argc++] = buf;
f0100859:	8d 7e 01             	lea    0x1(%esi),%edi
f010085c:	89 5c b5 a8          	mov    %ebx,-0x58(%ebp,%esi,4)
f0100860:	eb 03                	jmp    f0100865 <monitor+0x99>
		while(*buf && !strchr(WHITESPACE, *buf))
			buf++;
f0100862:	83 c3 01             	add    $0x1,%ebx
		if(argc == MAXARGS - 1){
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while(*buf && !strchr(WHITESPACE, *buf))
f0100865:	0f b6 03             	movzbl (%ebx),%eax
f0100868:	84 c0                	test   %al,%al
f010086a:	74 ae                	je     f010081a <monitor+0x4e>
f010086c:	83 ec 08             	sub    $0x8,%esp
f010086f:	0f be c0             	movsbl %al,%eax
f0100872:	50                   	push   %eax
f0100873:	68 80 1c 10 f0       	push   $0xf0101c80
f0100878:	e8 79 0b 00 00       	call   f01013f6 <strchr>
f010087d:	83 c4 10             	add    $0x10,%esp
f0100880:	85 c0                	test   %eax,%eax
f0100882:	74 de                	je     f0100862 <monitor+0x96>
f0100884:	eb 94                	jmp    f010081a <monitor+0x4e>
			buf++;
	}
	argv[argc] = 0;
f0100886:	c7 44 b5 a8 00 00 00 	movl   $0x0,-0x58(%ebp,%esi,4)
f010088d:	00 

	if(argc == 0)
f010088e:	85 f6                	test   %esi,%esi
f0100890:	0f 84 58 ff ff ff    	je     f01007ee <monitor+0x22>
		return 0;

	for(i = 0; i < ARRAY_SIZE(commands); i++){
		if(strcmp(argv[0], commands[i].name) == 0)
f0100896:	83 ec 08             	sub    $0x8,%esp
f0100899:	68 1e 1c 10 f0       	push   $0xf0101c1e
f010089e:	ff 75 a8             	pushl  -0x58(%ebp)
f01008a1:	e8 f2 0a 00 00       	call   f0101398 <strcmp>
f01008a6:	83 c4 10             	add    $0x10,%esp
f01008a9:	85 c0                	test   %eax,%eax
f01008ab:	74 1b                	je     f01008c8 <monitor+0xfc>
f01008ad:	83 ec 08             	sub    $0x8,%esp
f01008b0:	68 2c 1c 10 f0       	push   $0xf0101c2c
f01008b5:	ff 75 a8             	pushl  -0x58(%ebp)
f01008b8:	e8 db 0a 00 00       	call   f0101398 <strcmp>
f01008bd:	83 c4 10             	add    $0x10,%esp
f01008c0:	85 c0                	test   %eax,%eax
f01008c2:	75 2d                	jne    f01008f1 <monitor+0x125>
	argv[argc] = 0;

	if(argc == 0)
		return 0;

	for(i = 0; i < ARRAY_SIZE(commands); i++){
f01008c4:	b0 01                	mov    $0x1,%al
f01008c6:	eb 05                	jmp    f01008cd <monitor+0x101>
		if(strcmp(argv[0], commands[i].name) == 0)
f01008c8:	b8 00 00 00 00       	mov    $0x0,%eax
			return commands[i].func(argc, argv, tf);
f01008cd:	83 ec 04             	sub    $0x4,%esp
f01008d0:	8d 14 00             	lea    (%eax,%eax,1),%edx
f01008d3:	01 d0                	add    %edx,%eax
f01008d5:	ff 75 08             	pushl  0x8(%ebp)
f01008d8:	8d 4d a8             	lea    -0x58(%ebp),%ecx
f01008db:	51                   	push   %ecx
f01008dc:	56                   	push   %esi
f01008dd:	ff 14 85 24 1e 10 f0 	call   *-0xfefe1dc(,%eax,4)
	cprintf("Type 'help' for a list of commands.\n");

	while(1){
		buf = readline("K> ");
		if(buf != NULL){
			if(runcmd(buf, tf) < 0)
f01008e4:	83 c4 10             	add    $0x10,%esp
f01008e7:	85 c0                	test   %eax,%eax
f01008e9:	0f 89 ff fe ff ff    	jns    f01007ee <monitor+0x22>
f01008ef:	eb 18                	jmp    f0100909 <monitor+0x13d>

	for(i = 0; i < ARRAY_SIZE(commands); i++){
		if(strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
f01008f1:	83 ec 08             	sub    $0x8,%esp
f01008f4:	ff 75 a8             	pushl  -0x58(%ebp)
f01008f7:	68 a2 1c 10 f0       	push   $0xf0101ca2
f01008fc:	e8 49 00 00 00       	call   f010094a <cprintf>
f0100901:	83 c4 10             	add    $0x10,%esp
f0100904:	e9 e5 fe ff ff       	jmp    f01007ee <monitor+0x22>
		if(buf != NULL){
			if(runcmd(buf, tf) < 0)
				break;
		}
	}
}
f0100909:	8d 65 f4             	lea    -0xc(%ebp),%esp
f010090c:	5b                   	pop    %ebx
f010090d:	5e                   	pop    %esi
f010090e:	5f                   	pop    %edi
f010090f:	5d                   	pop    %ebp
f0100910:	c3                   	ret    

f0100911 <putch>:
#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/stdarg.h>

// 输出一个字符，cnt值 自增1
static void putch(int ch, int *cnt){
f0100911:	55                   	push   %ebp
f0100912:	89 e5                	mov    %esp,%ebp
f0100914:	83 ec 14             	sub    $0x14,%esp
	cputchar(ch);
f0100917:	ff 75 08             	pushl  0x8(%ebp)
f010091a:	e8 14 fd ff ff       	call   f0100633 <cputchar>
f010091f:	83 c4 10             	add    $0x10,%esp
	*cnt++;
}
f0100922:	c9                   	leave  
f0100923:	c3                   	ret    

f0100924 <vcprintf>:

// 输出一个字符串，返回输出字符数目
int vcprintf(const char *fmt, va_list ap){
f0100924:	55                   	push   %ebp
f0100925:	89 e5                	mov    %esp,%ebp
f0100927:	83 ec 18             	sub    $0x18,%esp
	int cnt = 0;
f010092a:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)
	
	vprintfmt((void*)putch, &cnt, fmt, ap);
f0100931:	ff 75 0c             	pushl  0xc(%ebp)
f0100934:	ff 75 08             	pushl  0x8(%ebp)
f0100937:	8d 45 f4             	lea    -0xc(%ebp),%eax
f010093a:	50                   	push   %eax
f010093b:	68 11 09 10 f0       	push   $0xf0100911
f0100940:	e8 7e 04 00 00       	call   f0100dc3 <vprintfmt>
	return cnt;
}
f0100945:	8b 45 f4             	mov    -0xc(%ebp),%eax
f0100948:	c9                   	leave  
f0100949:	c3                   	ret    

f010094a <cprintf>:

// 输出字符串，返回输出字符数目
int cprintf(const char *fmt, ...){
f010094a:	55                   	push   %ebp
f010094b:	89 e5                	mov    %esp,%ebp
f010094d:	83 ec 10             	sub    $0x10,%esp
	va_list ap;
	int cnt;

	va_start(ap, fmt);
f0100950:	8d 45 0c             	lea    0xc(%ebp),%eax
	cnt = vcprintf(fmt, ap);
f0100953:	50                   	push   %eax
f0100954:	ff 75 08             	pushl  0x8(%ebp)
f0100957:	e8 c8 ff ff ff       	call   f0100924 <vcprintf>
	va_end(ap);

	return cnt;
}
f010095c:	c9                   	leave  
f010095d:	c3                   	ret    

f010095e <stab_binsearch>:
 	left = 0, right = 657; 
 	stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
 	会设置 left = 118, right = 554。
 */
static void stab_binsearch(const struct Stab *stabs, int *region_left,
				int *region_right, int type, uintptr_t addr){
f010095e:	55                   	push   %ebp
f010095f:	89 e5                	mov    %esp,%ebp
f0100961:	57                   	push   %edi
f0100962:	56                   	push   %esi
f0100963:	53                   	push   %ebx
f0100964:	83 ec 14             	sub    $0x14,%esp
f0100967:	89 45 ec             	mov    %eax,-0x14(%ebp)
f010096a:	89 55 e4             	mov    %edx,-0x1c(%ebp)
f010096d:	89 4d e0             	mov    %ecx,-0x20(%ebp)
f0100970:	8b 7d 08             	mov    0x8(%ebp),%edi
	int l = *region_left, r = *region_right, any_matches = 0;
f0100973:	8b 1a                	mov    (%edx),%ebx
f0100975:	8b 01                	mov    (%ecx),%eax
f0100977:	89 45 f0             	mov    %eax,-0x10(%ebp)
f010097a:	c7 45 e8 00 00 00 00 	movl   $0x0,-0x18(%ebp)

	// 二分查找
	while(l <= r){
f0100981:	e9 88 00 00 00       	jmp    f0100a0e <stab_binsearch+0xb0>
		int true_m = (l + r) / 2, m = true_m;
f0100986:	8b 45 f0             	mov    -0x10(%ebp),%eax
f0100989:	01 d8                	add    %ebx,%eax
f010098b:	89 c6                	mov    %eax,%esi
f010098d:	c1 ee 1f             	shr    $0x1f,%esi
f0100990:	01 c6                	add    %eax,%esi
f0100992:	d1 fe                	sar    %esi
f0100994:	8d 04 76             	lea    (%esi,%esi,2),%eax
f0100997:	8b 4d ec             	mov    -0x14(%ebp),%ecx
f010099a:	8d 14 81             	lea    (%ecx,%eax,4),%edx
f010099d:	89 f0                	mov    %esi,%eax

		// 查找最早出现的类型为type的stab
		while(m >= l && stabs[m].n_type != type)
f010099f:	eb 03                	jmp    f01009a4 <stab_binsearch+0x46>
			m--;
f01009a1:	83 e8 01             	sub    $0x1,%eax
	// 二分查找
	while(l <= r){
		int true_m = (l + r) / 2, m = true_m;

		// 查找最早出现的类型为type的stab
		while(m >= l && stabs[m].n_type != type)
f01009a4:	39 c3                	cmp    %eax,%ebx
f01009a6:	7f 1f                	jg     f01009c7 <stab_binsearch+0x69>
f01009a8:	0f b6 4a 04          	movzbl 0x4(%edx),%ecx
f01009ac:	83 ea 0c             	sub    $0xc,%edx
f01009af:	39 f9                	cmp    %edi,%ecx
f01009b1:	75 ee                	jne    f01009a1 <stab_binsearch+0x43>
f01009b3:	89 45 e8             	mov    %eax,-0x18(%ebp)
			l = true_m + 1;
			continue;
		}

		any_matches = 1;
		if(stabs[m].n_value < addr){
f01009b6:	8d 14 40             	lea    (%eax,%eax,2),%edx
f01009b9:	8b 4d ec             	mov    -0x14(%ebp),%ecx
f01009bc:	8b 54 91 08          	mov    0x8(%ecx,%edx,4),%edx
f01009c0:	39 55 0c             	cmp    %edx,0xc(%ebp)
f01009c3:	76 18                	jbe    f01009dd <stab_binsearch+0x7f>
f01009c5:	eb 05                	jmp    f01009cc <stab_binsearch+0x6e>

		// 查找最早出现的类型为type的stab
		while(m >= l && stabs[m].n_type != type)
			m--;
		if(m < l){
			l = true_m + 1;
f01009c7:	8d 5e 01             	lea    0x1(%esi),%ebx
			continue;
f01009ca:	eb 42                	jmp    f0100a0e <stab_binsearch+0xb0>
		}

		any_matches = 1;
		if(stabs[m].n_value < addr){
			*region_left = m;
f01009cc:	8b 5d e4             	mov    -0x1c(%ebp),%ebx
f01009cf:	89 03                	mov    %eax,(%ebx)
			l = true_m + 1;
f01009d1:	8d 5e 01             	lea    0x1(%esi),%ebx
		if(m < l){
			l = true_m + 1;
			continue;
		}

		any_matches = 1;
f01009d4:	c7 45 e8 01 00 00 00 	movl   $0x1,-0x18(%ebp)
f01009db:	eb 31                	jmp    f0100a0e <stab_binsearch+0xb0>
		if(stabs[m].n_value < addr){
			*region_left = m;
			l = true_m + 1;
		}
		else if(stabs[m].n_value > addr){
f01009dd:	39 55 0c             	cmp    %edx,0xc(%ebp)
f01009e0:	73 17                	jae    f01009f9 <stab_binsearch+0x9b>
			*region_right = m - 1;
f01009e2:	8b 45 e8             	mov    -0x18(%ebp),%eax
f01009e5:	83 e8 01             	sub    $0x1,%eax
f01009e8:	89 45 f0             	mov    %eax,-0x10(%ebp)
f01009eb:	8b 75 e0             	mov    -0x20(%ebp),%esi
f01009ee:	89 06                	mov    %eax,(%esi)
		if(m < l){
			l = true_m + 1;
			continue;
		}

		any_matches = 1;
f01009f0:	c7 45 e8 01 00 00 00 	movl   $0x1,-0x18(%ebp)
f01009f7:	eb 15                	jmp    f0100a0e <stab_binsearch+0xb0>
		else if(stabs[m].n_value > addr){
			*region_right = m - 1;
			r = m - 1;
		}
		else{
			*region_left = m;	// 完全匹配addr，仍然继续查找
f01009f9:	8b 75 e4             	mov    -0x1c(%ebp),%esi
f01009fc:	8b 5d e8             	mov    -0x18(%ebp),%ebx
f01009ff:	89 1e                	mov    %ebx,(%esi)
			l = m;
			addr++;
f0100a01:	83 45 0c 01          	addl   $0x1,0xc(%ebp)
f0100a05:	89 c3                	mov    %eax,%ebx
		if(m < l){
			l = true_m + 1;
			continue;
		}

		any_matches = 1;
f0100a07:	c7 45 e8 01 00 00 00 	movl   $0x1,-0x18(%ebp)
static void stab_binsearch(const struct Stab *stabs, int *region_left,
				int *region_right, int type, uintptr_t addr){
	int l = *region_left, r = *region_right, any_matches = 0;

	// 二分查找
	while(l <= r){
f0100a0e:	3b 5d f0             	cmp    -0x10(%ebp),%ebx
f0100a11:	0f 8e 6f ff ff ff    	jle    f0100986 <stab_binsearch+0x28>
			l = m;
			addr++;
		}
	}

	if(!any_matches)
f0100a17:	83 7d e8 00          	cmpl   $0x0,-0x18(%ebp)
f0100a1b:	75 0f                	jne    f0100a2c <stab_binsearch+0xce>
		*region_right = *region_left - 1;
f0100a1d:	8b 45 e4             	mov    -0x1c(%ebp),%eax
f0100a20:	8b 00                	mov    (%eax),%eax
f0100a22:	83 e8 01             	sub    $0x1,%eax
f0100a25:	8b 75 e0             	mov    -0x20(%ebp),%esi
f0100a28:	89 06                	mov    %eax,(%esi)
f0100a2a:	eb 2c                	jmp    f0100a58 <stab_binsearch+0xfa>
	else{
		// 发现包含'addr'最右边的区域
		for(l = *region_right; l > *region_left && stabs[l].n_type != type; l--)
f0100a2c:	8b 45 e0             	mov    -0x20(%ebp),%eax
f0100a2f:	8b 00                	mov    (%eax),%eax
f0100a31:	8b 75 e4             	mov    -0x1c(%ebp),%esi
f0100a34:	8b 0e                	mov    (%esi),%ecx
f0100a36:	8d 14 40             	lea    (%eax,%eax,2),%edx
f0100a39:	8b 75 ec             	mov    -0x14(%ebp),%esi
f0100a3c:	8d 14 96             	lea    (%esi,%edx,4),%edx
f0100a3f:	eb 03                	jmp    f0100a44 <stab_binsearch+0xe6>
f0100a41:	83 e8 01             	sub    $0x1,%eax
f0100a44:	39 c8                	cmp    %ecx,%eax
f0100a46:	7e 0b                	jle    f0100a53 <stab_binsearch+0xf5>
f0100a48:	0f b6 5a 04          	movzbl 0x4(%edx),%ebx
f0100a4c:	83 ea 0c             	sub    $0xc,%edx
f0100a4f:	39 fb                	cmp    %edi,%ebx
f0100a51:	75 ee                	jne    f0100a41 <stab_binsearch+0xe3>
			;
		*region_left = l;
f0100a53:	8b 75 e4             	mov    -0x1c(%ebp),%esi
f0100a56:	89 06                	mov    %eax,(%esi)
	}
}
f0100a58:	83 c4 14             	add    $0x14,%esp
f0100a5b:	5b                   	pop    %ebx
f0100a5c:	5e                   	pop    %esi
f0100a5d:	5f                   	pop    %edi
f0100a5e:	5d                   	pop    %ebp
f0100a5f:	c3                   	ret    

f0100a60 <debuginfo_eip>:
/*
 debuginfo_eip(addr, info);
 用指定EIP指令地址addr处的信息，填充 info 结构体；
 如果找到eip信息返回0；否则返回负值，同时在info中也会储存一些信息。
 */
int debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info){
f0100a60:	55                   	push   %ebp
f0100a61:	89 e5                	mov    %esp,%ebp
f0100a63:	57                   	push   %edi
f0100a64:	56                   	push   %esi
f0100a65:	53                   	push   %ebx
f0100a66:	83 ec 3c             	sub    $0x3c,%esp
f0100a69:	8b 75 08             	mov    0x8(%ebp),%esi
f0100a6c:	8b 5d 0c             	mov    0xc(%ebp),%ebx
	const char *stabstr, *stabstr_end;

	int lfile, rfile, lfun, rfun, lline, rline;

	// 初始化 info
	info->eip_file = "<unknown>";
f0100a6f:	c7 03 34 1e 10 f0    	movl   $0xf0101e34,(%ebx)
	info->eip_line = 0;
f0100a75:	c7 43 04 00 00 00 00 	movl   $0x0,0x4(%ebx)
	info->eip_fn_name = "<unknown>";
f0100a7c:	c7 43 08 34 1e 10 f0 	movl   $0xf0101e34,0x8(%ebx)
	info->eip_fn_namelen = 9;
f0100a83:	c7 43 0c 09 00 00 00 	movl   $0x9,0xc(%ebx)
	info->eip_fn_addr = addr;
f0100a8a:	89 73 10             	mov    %esi,0x10(%ebx)
	info->eip_fn_narg = 0;
f0100a8d:	c7 43 14 00 00 00 00 	movl   $0x0,0x14(%ebx)

	if(addr >= ULIM){
f0100a94:	81 fe ff ff 7f ef    	cmp    $0xef7fffff,%esi
f0100a9a:	76 11                	jbe    f0100aad <debuginfo_eip+0x4d>
		// 不能搜索用户级地址
		panic("User address");
	}

	// 字符串表的有效性检查
	if(stabstr_end <= stabstr || stabstr_end[-1] != 0)
f0100a9c:	b8 6a 72 10 f0       	mov    $0xf010726a,%eax
f0100aa1:	3d 85 59 10 f0       	cmp    $0xf0105985,%eax
f0100aa6:	77 19                	ja     f0100ac1 <debuginfo_eip+0x61>
f0100aa8:	e9 a9 01 00 00       	jmp    f0100c56 <debuginfo_eip+0x1f6>
		stabstr = __STABSTR_BEGIN__;
		stabstr_end = __STABSTR_END__;
	}
	else{
		// 不能搜索用户级地址
		panic("User address");
f0100aad:	83 ec 04             	sub    $0x4,%esp
f0100ab0:	68 3e 1e 10 f0       	push   $0xf0101e3e
f0100ab5:	6a 71                	push   $0x71
f0100ab7:	68 4b 1e 10 f0       	push   $0xf0101e4b
f0100abc:	e8 25 f6 ff ff       	call   f01000e6 <_panic>
	}

	// 字符串表的有效性检查
	if(stabstr_end <= stabstr || stabstr_end[-1] != 0)
f0100ac1:	80 3d 69 72 10 f0 00 	cmpb   $0x0,0xf0107269
f0100ac8:	0f 85 8f 01 00 00    	jne    f0100c5d <debuginfo_eip+0x1fd>
	// 现在我们开始查找包含eip指令的函数所在的stab表；
	// 首先，查找包含eip指令的基本的源文件，然后在源文件中查找函数；
	// 最后，找到eip指令所在的文件行。
	
	// 查找源文件全部的类型的 N_SO 的 stabs表
	lfile = 0;
f0100ace:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
	rfile = (stab_end - stabs) - 1;
f0100ad5:	b8 84 59 10 f0       	mov    $0xf0105984,%eax
f0100ada:	2d 6c 20 10 f0       	sub    $0xf010206c,%eax
f0100adf:	c1 f8 02             	sar    $0x2,%eax
f0100ae2:	69 c0 ab aa aa aa    	imul   $0xaaaaaaab,%eax,%eax
f0100ae8:	83 e8 01             	sub    $0x1,%eax
f0100aeb:	89 45 e0             	mov    %eax,-0x20(%ebp)
	stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
f0100aee:	83 ec 08             	sub    $0x8,%esp
f0100af1:	56                   	push   %esi
f0100af2:	6a 64                	push   $0x64
f0100af4:	8d 4d e0             	lea    -0x20(%ebp),%ecx
f0100af7:	8d 55 e4             	lea    -0x1c(%ebp),%edx
f0100afa:	b8 6c 20 10 f0       	mov    $0xf010206c,%eax
f0100aff:	e8 5a fe ff ff       	call   f010095e <stab_binsearch>
	if(lfile == 0)
f0100b04:	8b 45 e4             	mov    -0x1c(%ebp),%eax
f0100b07:	83 c4 10             	add    $0x10,%esp
f0100b0a:	85 c0                	test   %eax,%eax
f0100b0c:	0f 84 52 01 00 00    	je     f0100c64 <debuginfo_eip+0x204>
		return -1;

	// 搜索源文件的stabs中函数定义
	lfun = lfile;
f0100b12:	89 45 dc             	mov    %eax,-0x24(%ebp)
	rfun = rfile;
f0100b15:	8b 45 e0             	mov    -0x20(%ebp),%eax
f0100b18:	89 45 d8             	mov    %eax,-0x28(%ebp)
	stab_binsearch(stabs, &lfun, &rfun, N_FUN, addr);
f0100b1b:	83 ec 08             	sub    $0x8,%esp
f0100b1e:	56                   	push   %esi
f0100b1f:	6a 24                	push   $0x24
f0100b21:	8d 4d d8             	lea    -0x28(%ebp),%ecx
f0100b24:	8d 55 dc             	lea    -0x24(%ebp),%edx
f0100b27:	b8 6c 20 10 f0       	mov    $0xf010206c,%eax
f0100b2c:	e8 2d fe ff ff       	call   f010095e <stab_binsearch>

	if(lfun <= rfun){
f0100b31:	8b 45 dc             	mov    -0x24(%ebp),%eax
f0100b34:	8b 55 d8             	mov    -0x28(%ebp),%edx
f0100b37:	83 c4 10             	add    $0x10,%esp
f0100b3a:	39 d0                	cmp    %edx,%eax
f0100b3c:	7f 40                	jg     f0100b7e <debuginfo_eip+0x11e>
		// stabs[lfun] 指向字符串表内的函数名
		if(stabs[lfun].n_strx < stabstr_end - stabstr)
f0100b3e:	8d 0c 40             	lea    (%eax,%eax,2),%ecx
f0100b41:	c1 e1 02             	shl    $0x2,%ecx
f0100b44:	8d b9 6c 20 10 f0    	lea    -0xfefdf94(%ecx),%edi
f0100b4a:	89 7d c4             	mov    %edi,-0x3c(%ebp)
f0100b4d:	8b b9 6c 20 10 f0    	mov    -0xfefdf94(%ecx),%edi
f0100b53:	b9 6a 72 10 f0       	mov    $0xf010726a,%ecx
f0100b58:	81 e9 85 59 10 f0    	sub    $0xf0105985,%ecx
f0100b5e:	39 cf                	cmp    %ecx,%edi
f0100b60:	73 09                	jae    f0100b6b <debuginfo_eip+0x10b>
			info->eip_fn_name = stabstr + stabs[lfun].n_strx;
f0100b62:	81 c7 85 59 10 f0    	add    $0xf0105985,%edi
f0100b68:	89 7b 08             	mov    %edi,0x8(%ebx)
		info->eip_fn_addr = stabs[lfun].n_value;
f0100b6b:	8b 7d c4             	mov    -0x3c(%ebp),%edi
f0100b6e:	8b 4f 08             	mov    0x8(%edi),%ecx
f0100b71:	89 4b 10             	mov    %ecx,0x10(%ebx)
		addr -= info->eip_fn_addr;
f0100b74:	29 ce                	sub    %ecx,%esi

		// 查找该函数内的行号
		lline = lfun;
f0100b76:	89 45 d4             	mov    %eax,-0x2c(%ebp)
		rline = rfun;
f0100b79:	89 55 d0             	mov    %edx,-0x30(%ebp)
f0100b7c:	eb 0f                	jmp    f0100b8d <debuginfo_eip+0x12d>
	}
	else {
		// 没有找到函数stab，可能在汇编文件中，此时查找整个文件的文件行
		info->eip_fn_addr = addr;
f0100b7e:	89 73 10             	mov    %esi,0x10(%ebx)
		lline = lfile;
f0100b81:	8b 45 e4             	mov    -0x1c(%ebp),%eax
f0100b84:	89 45 d4             	mov    %eax,-0x2c(%ebp)
		rline = rfile;
f0100b87:	8b 45 e0             	mov    -0x20(%ebp),%eax
f0100b8a:	89 45 d0             	mov    %eax,-0x30(%ebp)
	}
	// 忽略冒号后的东西
	info->eip_fn_namelen = strfind(info->eip_fn_name, ':') - info->eip_fn_name;
f0100b8d:	83 ec 08             	sub    $0x8,%esp
f0100b90:	6a 3a                	push   $0x3a
f0100b92:	ff 73 08             	pushl  0x8(%ebx)
f0100b95:	e8 7d 08 00 00       	call   f0101417 <strfind>
f0100b9a:	2b 43 08             	sub    0x8(%ebx),%eax
f0100b9d:	89 43 0c             	mov    %eax,0xc(%ebx)

	// 在 [lline, rline]中查找文件行stab，
	// 如果找到了，就设置 info->eip_line 为文件行，否则返回 -1
	// 注意：文件行有特殊的 stabs类型，在 <inc/stab.h> 中定义
	
	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
f0100ba0:	83 c4 08             	add    $0x8,%esp
f0100ba3:	56                   	push   %esi
f0100ba4:	6a 44                	push   $0x44
f0100ba6:	8d 4d d0             	lea    -0x30(%ebp),%ecx
f0100ba9:	8d 55 d4             	lea    -0x2c(%ebp),%edx
f0100bac:	b8 6c 20 10 f0       	mov    $0xf010206c,%eax
f0100bb1:	e8 a8 fd ff ff       	call   f010095e <stab_binsearch>
	if(lline <= rline)
f0100bb6:	8b 45 d4             	mov    -0x2c(%ebp),%eax
f0100bb9:	83 c4 10             	add    $0x10,%esp
f0100bbc:	3b 45 d0             	cmp    -0x30(%ebp),%eax
f0100bbf:	0f 8f a6 00 00 00    	jg     f0100c6b <debuginfo_eip+0x20b>
		info->eip_line = stabs[lline].n_desc;
f0100bc5:	8d 04 40             	lea    (%eax,%eax,2),%eax
f0100bc8:	0f b7 04 85 72 20 10 	movzwl -0xfefdf8e(,%eax,4),%eax
f0100bcf:	f0 
f0100bd0:	89 43 04             	mov    %eax,0x4(%ebx)
		return -1;

	// 从相关文件名的文件行stab向后搜索。
	// 我们不能仅使用 lfile stab，因为内联函数从不同文件插入代码。
	// 这些包含的源文件使用 N_SOL stab类型
	while(lline >= lfile && stabs[lline].n_type != N_SOL
f0100bd3:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f0100bd6:	8b 45 d4             	mov    -0x2c(%ebp),%eax
f0100bd9:	8d 14 40             	lea    (%eax,%eax,2),%edx
f0100bdc:	8d 14 95 6c 20 10 f0 	lea    -0xfefdf94(,%edx,4),%edx
f0100be3:	89 5d 0c             	mov    %ebx,0xc(%ebp)
f0100be6:	eb 06                	jmp    f0100bee <debuginfo_eip+0x18e>
f0100be8:	83 e8 01             	sub    $0x1,%eax
f0100beb:	83 ea 0c             	sub    $0xc,%edx
f0100bee:	39 c7                	cmp    %eax,%edi
f0100bf0:	7f 23                	jg     f0100c15 <debuginfo_eip+0x1b5>
f0100bf2:	0f b6 4a 04          	movzbl 0x4(%edx),%ecx
f0100bf6:	80 f9 84             	cmp    $0x84,%cl
f0100bf9:	74 7e                	je     f0100c79 <debuginfo_eip+0x219>
		&& (stabs[lline].n_type != N_SO || !stabs[lline].n_value))
f0100bfb:	80 f9 64             	cmp    $0x64,%cl
f0100bfe:	75 e8                	jne    f0100be8 <debuginfo_eip+0x188>
f0100c00:	83 7a 08 00          	cmpl   $0x0,0x8(%edx)
f0100c04:	74 e2                	je     f0100be8 <debuginfo_eip+0x188>
f0100c06:	8b 5d 0c             	mov    0xc(%ebp),%ebx
f0100c09:	eb 71                	jmp    f0100c7c <debuginfo_eip+0x21c>
		lline--;
	if(lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
		info->eip_file = stabstr + stabs[lline].n_strx;
f0100c0b:	81 c2 85 59 10 f0    	add    $0xf0105985,%edx
f0100c11:	89 13                	mov    %edx,(%ebx)
f0100c13:	eb 03                	jmp    f0100c18 <debuginfo_eip+0x1b8>
f0100c15:	8b 5d 0c             	mov    0xc(%ebp),%ebx

	// 设置 eip_fn_narg为函数的参数数目，如果没有函数则设置为0
	if(lfun < rfun){
f0100c18:	8b 55 dc             	mov    -0x24(%ebp),%edx
f0100c1b:	8b 75 d8             	mov    -0x28(%ebp),%esi
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
			info->eip_fn_narg++;
	}
	return 0;
f0100c1e:	b8 00 00 00 00       	mov    $0x0,%eax
		lline--;
	if(lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
		info->eip_file = stabstr + stabs[lline].n_strx;

	// 设置 eip_fn_narg为函数的参数数目，如果没有函数则设置为0
	if(lfun < rfun){
f0100c23:	39 f2                	cmp    %esi,%edx
f0100c25:	7d 76                	jge    f0100c9d <debuginfo_eip+0x23d>
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
f0100c27:	83 c2 01             	add    $0x1,%edx
f0100c2a:	89 d0                	mov    %edx,%eax
f0100c2c:	8d 14 52             	lea    (%edx,%edx,2),%edx
f0100c2f:	8d 14 95 6c 20 10 f0 	lea    -0xfefdf94(,%edx,4),%edx
f0100c36:	eb 04                	jmp    f0100c3c <debuginfo_eip+0x1dc>
			info->eip_fn_narg++;
f0100c38:	83 43 14 01          	addl   $0x1,0x14(%ebx)
	if(lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
		info->eip_file = stabstr + stabs[lline].n_strx;

	// 设置 eip_fn_narg为函数的参数数目，如果没有函数则设置为0
	if(lfun < rfun){
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
f0100c3c:	39 c6                	cmp    %eax,%esi
f0100c3e:	7e 32                	jle    f0100c72 <debuginfo_eip+0x212>
f0100c40:	0f b6 4a 04          	movzbl 0x4(%edx),%ecx
f0100c44:	83 c0 01             	add    $0x1,%eax
f0100c47:	83 c2 0c             	add    $0xc,%edx
f0100c4a:	80 f9 a0             	cmp    $0xa0,%cl
f0100c4d:	74 e9                	je     f0100c38 <debuginfo_eip+0x1d8>
			info->eip_fn_narg++;
	}
	return 0;
f0100c4f:	b8 00 00 00 00       	mov    $0x0,%eax
f0100c54:	eb 47                	jmp    f0100c9d <debuginfo_eip+0x23d>
		panic("User address");
	}

	// 字符串表的有效性检查
	if(stabstr_end <= stabstr || stabstr_end[-1] != 0)
		return -1;
f0100c56:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f0100c5b:	eb 40                	jmp    f0100c9d <debuginfo_eip+0x23d>
f0100c5d:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f0100c62:	eb 39                	jmp    f0100c9d <debuginfo_eip+0x23d>
	// 查找源文件全部的类型的 N_SO 的 stabs表
	lfile = 0;
	rfile = (stab_end - stabs) - 1;
	stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
	if(lfile == 0)
		return -1;
f0100c64:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f0100c69:	eb 32                	jmp    f0100c9d <debuginfo_eip+0x23d>
	
	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
	if(lline <= rline)
		info->eip_line = stabs[lline].n_desc;
	else
		return -1;
f0100c6b:	b8 ff ff ff ff       	mov    $0xffffffff,%eax
f0100c70:	eb 2b                	jmp    f0100c9d <debuginfo_eip+0x23d>
	// 设置 eip_fn_narg为函数的参数数目，如果没有函数则设置为0
	if(lfun < rfun){
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
			info->eip_fn_narg++;
	}
	return 0;
f0100c72:	b8 00 00 00 00       	mov    $0x0,%eax
f0100c77:	eb 24                	jmp    f0100c9d <debuginfo_eip+0x23d>
f0100c79:	8b 5d 0c             	mov    0xc(%ebp),%ebx
	// 我们不能仅使用 lfile stab，因为内联函数从不同文件插入代码。
	// 这些包含的源文件使用 N_SOL stab类型
	while(lline >= lfile && stabs[lline].n_type != N_SOL
		&& (stabs[lline].n_type != N_SO || !stabs[lline].n_value))
		lline--;
	if(lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
f0100c7c:	8d 04 40             	lea    (%eax,%eax,2),%eax
f0100c7f:	8b 14 85 6c 20 10 f0 	mov    -0xfefdf94(,%eax,4),%edx
f0100c86:	b8 6a 72 10 f0       	mov    $0xf010726a,%eax
f0100c8b:	2d 85 59 10 f0       	sub    $0xf0105985,%eax
f0100c90:	39 c2                	cmp    %eax,%edx
f0100c92:	0f 82 73 ff ff ff    	jb     f0100c0b <debuginfo_eip+0x1ab>
f0100c98:	e9 7b ff ff ff       	jmp    f0100c18 <debuginfo_eip+0x1b8>
	if(lfun < rfun){
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
			info->eip_fn_narg++;
	}
	return 0;
}
f0100c9d:	8d 65 f4             	lea    -0xc(%ebp),%esp
f0100ca0:	5b                   	pop    %ebx
f0100ca1:	5e                   	pop    %esi
f0100ca2:	5f                   	pop    %edi
f0100ca3:	5d                   	pop    %ebp
f0100ca4:	c3                   	ret    

f0100ca5 <printnum>:
 * base:		输出进制
 * width:	输出字符宽度
 * padc:		宽度填充
 */
static void printnum(void (*putch)(int, void*), void *putdat, 
	unsigned long long num, unsigned base, int width, int padc){
f0100ca5:	55                   	push   %ebp
f0100ca6:	89 e5                	mov    %esp,%ebp
f0100ca8:	57                   	push   %edi
f0100ca9:	56                   	push   %esi
f0100caa:	53                   	push   %ebx
f0100cab:	83 ec 1c             	sub    $0x1c,%esp
f0100cae:	89 c7                	mov    %eax,%edi
f0100cb0:	89 d6                	mov    %edx,%esi
f0100cb2:	8b 45 08             	mov    0x8(%ebp),%eax
f0100cb5:	8b 55 0c             	mov    0xc(%ebp),%edx
f0100cb8:	89 d1                	mov    %edx,%ecx
f0100cba:	89 45 d8             	mov    %eax,-0x28(%ebp)
f0100cbd:	89 4d dc             	mov    %ecx,-0x24(%ebp)
f0100cc0:	8b 45 10             	mov    0x10(%ebp),%eax
f0100cc3:	8b 5d 14             	mov    0x14(%ebp),%ebx
	if(num >= base)
f0100cc6:	89 45 e0             	mov    %eax,-0x20(%ebp)
f0100cc9:	c7 45 e4 00 00 00 00 	movl   $0x0,-0x1c(%ebp)
f0100cd0:	39 4d e4             	cmp    %ecx,-0x1c(%ebp)
f0100cd3:	72 05                	jb     f0100cda <printnum+0x35>
f0100cd5:	3b 45 d8             	cmp    -0x28(%ebp),%eax
f0100cd8:	77 3e                	ja     f0100d18 <printnum+0x73>
		printnum(putch, putdat, num / base, base, width-1, padc);
f0100cda:	83 ec 0c             	sub    $0xc,%esp
f0100cdd:	ff 75 18             	pushl  0x18(%ebp)
f0100ce0:	83 eb 01             	sub    $0x1,%ebx
f0100ce3:	53                   	push   %ebx
f0100ce4:	50                   	push   %eax
f0100ce5:	83 ec 08             	sub    $0x8,%esp
f0100ce8:	ff 75 e4             	pushl  -0x1c(%ebp)
f0100ceb:	ff 75 e0             	pushl  -0x20(%ebp)
f0100cee:	ff 75 dc             	pushl  -0x24(%ebp)
f0100cf1:	ff 75 d8             	pushl  -0x28(%ebp)
f0100cf4:	e8 47 09 00 00       	call   f0101640 <__udivdi3>
f0100cf9:	83 c4 18             	add    $0x18,%esp
f0100cfc:	52                   	push   %edx
f0100cfd:	50                   	push   %eax
f0100cfe:	89 f2                	mov    %esi,%edx
f0100d00:	89 f8                	mov    %edi,%eax
f0100d02:	e8 9e ff ff ff       	call   f0100ca5 <printnum>
f0100d07:	83 c4 20             	add    $0x20,%esp
f0100d0a:	eb 13                	jmp    f0100d1f <printnum+0x7a>
	else{
		while(--width > 0)
			putch(padc, putdat);
f0100d0c:	83 ec 08             	sub    $0x8,%esp
f0100d0f:	56                   	push   %esi
f0100d10:	ff 75 18             	pushl  0x18(%ebp)
f0100d13:	ff d7                	call   *%edi
f0100d15:	83 c4 10             	add    $0x10,%esp
static void printnum(void (*putch)(int, void*), void *putdat, 
	unsigned long long num, unsigned base, int width, int padc){
	if(num >= base)
		printnum(putch, putdat, num / base, base, width-1, padc);
	else{
		while(--width > 0)
f0100d18:	83 eb 01             	sub    $0x1,%ebx
f0100d1b:	85 db                	test   %ebx,%ebx
f0100d1d:	7f ed                	jg     f0100d0c <printnum+0x67>
			putch(padc, putdat);
	}
	putch("0123456789abcdef"[num % base], putdat);
f0100d1f:	83 ec 08             	sub    $0x8,%esp
f0100d22:	56                   	push   %esi
f0100d23:	83 ec 04             	sub    $0x4,%esp
f0100d26:	ff 75 e4             	pushl  -0x1c(%ebp)
f0100d29:	ff 75 e0             	pushl  -0x20(%ebp)
f0100d2c:	ff 75 dc             	pushl  -0x24(%ebp)
f0100d2f:	ff 75 d8             	pushl  -0x28(%ebp)
f0100d32:	e8 39 0a 00 00       	call   f0101770 <__umoddi3>
f0100d37:	83 c4 14             	add    $0x14,%esp
f0100d3a:	0f be 80 59 1e 10 f0 	movsbl -0xfefe1a7(%eax),%eax
f0100d41:	50                   	push   %eax
f0100d42:	ff d7                	call   *%edi
f0100d44:	83 c4 10             	add    $0x10,%esp
}
f0100d47:	8d 65 f4             	lea    -0xc(%ebp),%esp
f0100d4a:	5b                   	pop    %ebx
f0100d4b:	5e                   	pop    %esi
f0100d4c:	5f                   	pop    %edi
f0100d4d:	5d                   	pop    %ebp
f0100d4e:	c3                   	ret    

f0100d4f <getuint>:


// 返回可变参数列表的各种可能的大小，取决于lflag标志参数
static unsigned long long getuint(va_list *ap, int lflag){
f0100d4f:	55                   	push   %ebp
f0100d50:	89 e5                	mov    %esp,%ebp
	if(lflag >= 2)
f0100d52:	83 fa 01             	cmp    $0x1,%edx
f0100d55:	7e 0e                	jle    f0100d65 <getuint+0x16>
		return va_arg(*ap, unsigned long long);
f0100d57:	8b 10                	mov    (%eax),%edx
f0100d59:	8d 4a 08             	lea    0x8(%edx),%ecx
f0100d5c:	89 08                	mov    %ecx,(%eax)
f0100d5e:	8b 02                	mov    (%edx),%eax
f0100d60:	8b 52 04             	mov    0x4(%edx),%edx
f0100d63:	eb 22                	jmp    f0100d87 <getuint+0x38>
	else if(lflag)
f0100d65:	85 d2                	test   %edx,%edx
f0100d67:	74 10                	je     f0100d79 <getuint+0x2a>
		return va_arg(*ap, unsigned long);
f0100d69:	8b 10                	mov    (%eax),%edx
f0100d6b:	8d 4a 04             	lea    0x4(%edx),%ecx
f0100d6e:	89 08                	mov    %ecx,(%eax)
f0100d70:	8b 02                	mov    (%edx),%eax
f0100d72:	ba 00 00 00 00       	mov    $0x0,%edx
f0100d77:	eb 0e                	jmp    f0100d87 <getuint+0x38>
	else
		return va_arg(*ap, unsigned int);
f0100d79:	8b 10                	mov    (%eax),%edx
f0100d7b:	8d 4a 04             	lea    0x4(%edx),%ecx
f0100d7e:	89 08                	mov    %ecx,(%eax)
f0100d80:	8b 02                	mov    (%edx),%eax
f0100d82:	ba 00 00 00 00       	mov    $0x0,%edx
}
f0100d87:	5d                   	pop    %ebp
f0100d88:	c3                   	ret    

f0100d89 <sprintputch>:
	char *ebuf;
	int cnt;
};

// 字符打印函数
static void sprintputch(int ch, struct sprintbuf *b){
f0100d89:	55                   	push   %ebp
f0100d8a:	89 e5                	mov    %esp,%ebp
f0100d8c:	8b 45 0c             	mov    0xc(%ebp),%eax
	b->cnt++;
f0100d8f:	83 40 08 01          	addl   $0x1,0x8(%eax)
	if(b->buf < b->ebuf)
f0100d93:	8b 10                	mov    (%eax),%edx
f0100d95:	3b 50 04             	cmp    0x4(%eax),%edx
f0100d98:	73 0a                	jae    f0100da4 <sprintputch+0x1b>
		*b->buf++ = ch;
f0100d9a:	8d 4a 01             	lea    0x1(%edx),%ecx
f0100d9d:	89 08                	mov    %ecx,(%eax)
f0100d9f:	8b 45 08             	mov    0x8(%ebp),%eax
f0100da2:	88 02                	mov    %al,(%edx)
}
f0100da4:	5d                   	pop    %ebp
f0100da5:	c3                   	ret    

f0100da6 <printfmt>:
		}
	}
}

// 格式化输出函数实现，外部调用版本
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...){
f0100da6:	55                   	push   %ebp
f0100da7:	89 e5                	mov    %esp,%ebp
f0100da9:	83 ec 08             	sub    $0x8,%esp
	va_list ap;

	va_start(ap, fmt);
f0100dac:	8d 45 14             	lea    0x14(%ebp),%eax
	vprintfmt(putch, putdat, fmt, ap);
f0100daf:	50                   	push   %eax
f0100db0:	ff 75 10             	pushl  0x10(%ebp)
f0100db3:	ff 75 0c             	pushl  0xc(%ebp)
f0100db6:	ff 75 08             	pushl  0x8(%ebp)
f0100db9:	e8 05 00 00 00       	call   f0100dc3 <vprintfmt>
	va_end(ap);
f0100dbe:	83 c4 10             	add    $0x10,%esp
}
f0100dc1:	c9                   	leave  
f0100dc2:	c3                   	ret    

f0100dc3 <vprintfmt>:
 * putch: 	输出单个字符的函数
 * putdat: 	记录输出字符数目
 * fmt:		格式化字符串
 * ap:		可变参数列表指针
 */
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap){
f0100dc3:	55                   	push   %ebp
f0100dc4:	89 e5                	mov    %esp,%ebp
f0100dc6:	57                   	push   %edi
f0100dc7:	56                   	push   %esi
f0100dc8:	53                   	push   %ebx
f0100dc9:	83 ec 2c             	sub    $0x2c,%esp
f0100dcc:	8b 75 08             	mov    0x8(%ebp),%esi
f0100dcf:	8b 5d 0c             	mov    0xc(%ebp),%ebx
f0100dd2:	8b 7d 10             	mov    0x10(%ebp),%edi
f0100dd5:	eb 12                	jmp    f0100de9 <vprintfmt+0x26>
		altflag;							// 小数点后精度控制
	char padc;							// 宽度填充

	while(1){
		while((ch = *(unsigned char *) fmt++) != '%'){
			if(ch == '\0')
f0100dd7:	85 c0                	test   %eax,%eax
f0100dd9:	0f 84 8d 03 00 00    	je     f010116c <vprintfmt+0x3a9>
				return;
			putch(ch, putdat);
f0100ddf:	83 ec 08             	sub    $0x8,%esp
f0100de2:	53                   	push   %ebx
f0100de3:	50                   	push   %eax
f0100de4:	ff d6                	call   *%esi
f0100de6:	83 c4 10             	add    $0x10,%esp
		precision,						// 输出精度控制
		altflag;							// 小数点后精度控制
	char padc;							// 宽度填充

	while(1){
		while((ch = *(unsigned char *) fmt++) != '%'){
f0100de9:	83 c7 01             	add    $0x1,%edi
f0100dec:	0f b6 47 ff          	movzbl -0x1(%edi),%eax
f0100df0:	83 f8 25             	cmp    $0x25,%eax
f0100df3:	75 e2                	jne    f0100dd7 <vprintfmt+0x14>
f0100df5:	c6 45 d4 20          	movb   $0x20,-0x2c(%ebp)
f0100df9:	c7 45 d8 00 00 00 00 	movl   $0x0,-0x28(%ebp)
f0100e00:	c7 45 d0 ff ff ff ff 	movl   $0xffffffff,-0x30(%ebp)
f0100e07:	c7 45 e0 ff ff ff ff 	movl   $0xffffffff,-0x20(%ebp)
f0100e0e:	ba 00 00 00 00       	mov    $0x0,%edx
f0100e13:	eb 07                	jmp    f0100e1c <vprintfmt+0x59>
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100e15:	8b 7d e4             	mov    -0x1c(%ebp),%edi
			case '-':					// 输出右对齐
				padc = '-';
f0100e18:	c6 45 d4 2d          	movb   $0x2d,-0x2c(%ebp)
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100e1c:	8d 47 01             	lea    0x1(%edi),%eax
f0100e1f:	89 45 e4             	mov    %eax,-0x1c(%ebp)
f0100e22:	0f b6 07             	movzbl (%edi),%eax
f0100e25:	0f b6 c8             	movzbl %al,%ecx
f0100e28:	83 e8 23             	sub    $0x23,%eax
f0100e2b:	3c 55                	cmp    $0x55,%al
f0100e2d:	0f 87 1e 03 00 00    	ja     f0101151 <vprintfmt+0x38e>
f0100e33:	0f b6 c0             	movzbl %al,%eax
f0100e36:	ff 24 85 e8 1e 10 f0 	jmp    *-0xfefe118(,%eax,4)
f0100e3d:	8b 7d e4             	mov    -0x1c(%ebp),%edi
			case '-':					// 输出右对齐
				padc = '-';
				goto reswitch;

			case '0':					// 宽度填充为 0
				padc = '0';
f0100e40:	c6 45 d4 30          	movb   $0x30,-0x2c(%ebp)
f0100e44:	eb d6                	jmp    f0100e1c <vprintfmt+0x59>
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100e46:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f0100e49:	b8 00 00 00 00       	mov    $0x0,%eax
f0100e4e:	89 55 e4             	mov    %edx,-0x1c(%ebp)
			case '6':
			case '7':
			case '8':
			case '9':
				for(precision = 0; ; ++fmt){
					precision = precision * 10 + ch - '0';
f0100e51:	8d 04 80             	lea    (%eax,%eax,4),%eax
f0100e54:	8d 44 41 d0          	lea    -0x30(%ecx,%eax,2),%eax
					ch = *fmt;
f0100e58:	0f be 0f             	movsbl (%edi),%ecx
					if(ch < '0' || ch > '9')
f0100e5b:	8d 51 d0             	lea    -0x30(%ecx),%edx
f0100e5e:	83 fa 09             	cmp    $0x9,%edx
f0100e61:	77 38                	ja     f0100e9b <vprintfmt+0xd8>
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				for(precision = 0; ; ++fmt){
f0100e63:	83 c7 01             	add    $0x1,%edi
					precision = precision * 10 + ch - '0';
					ch = *fmt;
					if(ch < '0' || ch > '9')
						break;
				}
f0100e66:	eb e9                	jmp    f0100e51 <vprintfmt+0x8e>
				goto process_precision;

			case '*':					// 域宽为*，取函数值为域宽
				precision = va_arg(ap, int);
f0100e68:	8b 45 14             	mov    0x14(%ebp),%eax
f0100e6b:	8d 48 04             	lea    0x4(%eax),%ecx
f0100e6e:	89 4d 14             	mov    %ecx,0x14(%ebp)
f0100e71:	8b 00                	mov    (%eax),%eax
f0100e73:	89 45 d0             	mov    %eax,-0x30(%ebp)
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100e76:	8b 7d e4             	mov    -0x1c(%ebp),%edi
				}
				goto process_precision;

			case '*':					// 域宽为*，取函数值为域宽
				precision = va_arg(ap, int);
				goto process_precision;
f0100e79:	eb 26                	jmp    f0100ea1 <vprintfmt+0xde>
f0100e7b:	8b 4d e0             	mov    -0x20(%ebp),%ecx
f0100e7e:	89 c8                	mov    %ecx,%eax
f0100e80:	c1 f8 1f             	sar    $0x1f,%eax
f0100e83:	f7 d0                	not    %eax
f0100e85:	21 c1                	and    %eax,%ecx
f0100e87:	89 4d e0             	mov    %ecx,-0x20(%ebp)
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100e8a:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f0100e8d:	eb 8d                	jmp    f0100e1c <vprintfmt+0x59>
f0100e8f:	8b 7d e4             	mov    -0x1c(%ebp),%edi
				if(width < 0)
					width = 0;
				goto reswitch;

			case '#':					// 显示尾部0，表示精度
				altflag = 1;
f0100e92:	c7 45 d8 01 00 00 00 	movl   $0x1,-0x28(%ebp)
				goto reswitch;
f0100e99:	eb 81                	jmp    f0100e1c <vprintfmt+0x59>
f0100e9b:	8b 55 e4             	mov    -0x1c(%ebp),%edx
f0100e9e:	89 45 d0             	mov    %eax,-0x30(%ebp)

			process_precision:			// 处理精度
				if(width < 0)
f0100ea1:	83 7d e0 00          	cmpl   $0x0,-0x20(%ebp)
f0100ea5:	0f 89 71 ff ff ff    	jns    f0100e1c <vprintfmt+0x59>
					width = precision, precision = -1;
f0100eab:	8b 45 d0             	mov    -0x30(%ebp),%eax
f0100eae:	89 45 e0             	mov    %eax,-0x20(%ebp)
f0100eb1:	c7 45 d0 ff ff ff ff 	movl   $0xffffffff,-0x30(%ebp)
f0100eb8:	e9 5f ff ff ff       	jmp    f0100e1c <vprintfmt+0x59>
				goto reswitch;

			case 'l':					// 输出为long long
				lflag++;
f0100ebd:	83 c2 01             	add    $0x1,%edx
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100ec0:	8b 7d e4             	mov    -0x1c(%ebp),%edi
					width = precision, precision = -1;
				goto reswitch;

			case 'l':					// 输出为long long
				lflag++;
				goto reswitch;
f0100ec3:	e9 54 ff ff ff       	jmp    f0100e1c <vprintfmt+0x59>

			case 'c':					// 输出单个字符
				putch(va_arg(ap, int), putdat);
f0100ec8:	8b 45 14             	mov    0x14(%ebp),%eax
f0100ecb:	8d 50 04             	lea    0x4(%eax),%edx
f0100ece:	89 55 14             	mov    %edx,0x14(%ebp)
f0100ed1:	83 ec 08             	sub    $0x8,%esp
f0100ed4:	53                   	push   %ebx
f0100ed5:	ff 30                	pushl  (%eax)
f0100ed7:	ff d6                	call   *%esi
				break;
f0100ed9:	83 c4 10             	add    $0x10,%esp
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100edc:	8b 7d e4             	mov    -0x1c(%ebp),%edi
				lflag++;
				goto reswitch;

			case 'c':					// 输出单个字符
				putch(va_arg(ap, int), putdat);
				break;
f0100edf:	e9 05 ff ff ff       	jmp    f0100de9 <vprintfmt+0x26>

			case 'e':					// 输出错误信息
				err = va_arg(ap, int);
f0100ee4:	8b 45 14             	mov    0x14(%ebp),%eax
f0100ee7:	8d 50 04             	lea    0x4(%eax),%edx
f0100eea:	89 55 14             	mov    %edx,0x14(%ebp)
f0100eed:	8b 00                	mov    (%eax),%eax
f0100eef:	99                   	cltd   
f0100ef0:	31 d0                	xor    %edx,%eax
f0100ef2:	29 d0                	sub    %edx,%eax
				if(err < 0)
					err = -err;
				if(err >= MAXERROR || (p = error_string[err]) == NULL)
f0100ef4:	83 f8 06             	cmp    $0x6,%eax
f0100ef7:	7f 0b                	jg     f0100f04 <vprintfmt+0x141>
f0100ef9:	8b 14 85 40 20 10 f0 	mov    -0xfefdfc0(,%eax,4),%edx
f0100f00:	85 d2                	test   %edx,%edx
f0100f02:	75 18                	jne    f0100f1c <vprintfmt+0x159>
					// 无错误描述，直接输出错误代码
					printfmt(putch, putdat, "error %d", err);
f0100f04:	50                   	push   %eax
f0100f05:	68 71 1e 10 f0       	push   $0xf0101e71
f0100f0a:	53                   	push   %ebx
f0100f0b:	56                   	push   %esi
f0100f0c:	e8 95 fe ff ff       	call   f0100da6 <printfmt>
f0100f11:	83 c4 10             	add    $0x10,%esp
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100f14:	8b 7d e4             	mov    -0x1c(%ebp),%edi
				err = va_arg(ap, int);
				if(err < 0)
					err = -err;
				if(err >= MAXERROR || (p = error_string[err]) == NULL)
					// 无错误描述，直接输出错误代码
					printfmt(putch, putdat, "error %d", err);
f0100f17:	e9 cd fe ff ff       	jmp    f0100de9 <vprintfmt+0x26>
				else
					printfmt(putch, putdat, "%s", p);
f0100f1c:	52                   	push   %edx
f0100f1d:	68 7a 1e 10 f0       	push   $0xf0101e7a
f0100f22:	53                   	push   %ebx
f0100f23:	56                   	push   %esi
f0100f24:	e8 7d fe ff ff       	call   f0100da6 <printfmt>
f0100f29:	83 c4 10             	add    $0x10,%esp
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0100f2c:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f0100f2f:	e9 b5 fe ff ff       	jmp    f0100de9 <vprintfmt+0x26>
f0100f34:	8b 4d d0             	mov    -0x30(%ebp),%ecx
f0100f37:	8b 45 e0             	mov    -0x20(%ebp),%eax
f0100f3a:	89 45 cc             	mov    %eax,-0x34(%ebp)
				else
					printfmt(putch, putdat, "%s", p);
				break;

			case 's':					// 输出字符串
				if((p = va_arg(ap, char*)) == NULL)
f0100f3d:	8b 45 14             	mov    0x14(%ebp),%eax
f0100f40:	8d 50 04             	lea    0x4(%eax),%edx
f0100f43:	89 55 14             	mov    %edx,0x14(%ebp)
f0100f46:	8b 38                	mov    (%eax),%edi
f0100f48:	85 ff                	test   %edi,%edi
f0100f4a:	75 05                	jne    f0100f51 <vprintfmt+0x18e>
					p = "(null)";
f0100f4c:	bf 6a 1e 10 f0       	mov    $0xf0101e6a,%edi
				if(width > 0 && padc != '-'){
f0100f51:	80 7d d4 2d          	cmpb   $0x2d,-0x2c(%ebp)
f0100f55:	0f 84 91 00 00 00    	je     f0100fec <vprintfmt+0x229>
f0100f5b:	83 7d cc 00          	cmpl   $0x0,-0x34(%ebp)
f0100f5f:	0f 8e 95 00 00 00    	jle    f0100ffa <vprintfmt+0x237>
					for(width -= strnlen(p, precision); width > 0; width--)
f0100f65:	83 ec 08             	sub    $0x8,%esp
f0100f68:	51                   	push   %ecx
f0100f69:	57                   	push   %edi
f0100f6a:	e8 5e 03 00 00       	call   f01012cd <strnlen>
f0100f6f:	8b 4d cc             	mov    -0x34(%ebp),%ecx
f0100f72:	29 c1                	sub    %eax,%ecx
f0100f74:	89 4d cc             	mov    %ecx,-0x34(%ebp)
f0100f77:	83 c4 10             	add    $0x10,%esp
						putch(padc, putdat);
f0100f7a:	0f be 45 d4          	movsbl -0x2c(%ebp),%eax
f0100f7e:	89 45 e0             	mov    %eax,-0x20(%ebp)
f0100f81:	89 7d d4             	mov    %edi,-0x2c(%ebp)
f0100f84:	89 cf                	mov    %ecx,%edi

			case 's':					// 输出字符串
				if((p = va_arg(ap, char*)) == NULL)
					p = "(null)";
				if(width > 0 && padc != '-'){
					for(width -= strnlen(p, precision); width > 0; width--)
f0100f86:	eb 0f                	jmp    f0100f97 <vprintfmt+0x1d4>
						putch(padc, putdat);
f0100f88:	83 ec 08             	sub    $0x8,%esp
f0100f8b:	53                   	push   %ebx
f0100f8c:	ff 75 e0             	pushl  -0x20(%ebp)
f0100f8f:	ff d6                	call   *%esi

			case 's':					// 输出字符串
				if((p = va_arg(ap, char*)) == NULL)
					p = "(null)";
				if(width > 0 && padc != '-'){
					for(width -= strnlen(p, precision); width > 0; width--)
f0100f91:	83 ef 01             	sub    $0x1,%edi
f0100f94:	83 c4 10             	add    $0x10,%esp
f0100f97:	85 ff                	test   %edi,%edi
f0100f99:	7f ed                	jg     f0100f88 <vprintfmt+0x1c5>
f0100f9b:	8b 7d d4             	mov    -0x2c(%ebp),%edi
f0100f9e:	8b 4d cc             	mov    -0x34(%ebp),%ecx
f0100fa1:	89 c8                	mov    %ecx,%eax
f0100fa3:	c1 f8 1f             	sar    $0x1f,%eax
f0100fa6:	f7 d0                	not    %eax
f0100fa8:	21 c8                	and    %ecx,%eax
f0100faa:	29 c1                	sub    %eax,%ecx
f0100fac:	89 75 08             	mov    %esi,0x8(%ebp)
f0100faf:	8b 75 d0             	mov    -0x30(%ebp),%esi
f0100fb2:	89 5d 0c             	mov    %ebx,0xc(%ebp)
f0100fb5:	89 cb                	mov    %ecx,%ebx
f0100fb7:	eb 4d                	jmp    f0101006 <vprintfmt+0x243>
						putch(padc, putdat);
				}
				for(; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--){
					if(altflag && (ch < ' ' || ch > '~'))
f0100fb9:	83 7d d8 00          	cmpl   $0x0,-0x28(%ebp)
f0100fbd:	74 1b                	je     f0100fda <vprintfmt+0x217>
f0100fbf:	0f be c0             	movsbl %al,%eax
f0100fc2:	83 e8 20             	sub    $0x20,%eax
f0100fc5:	83 f8 5e             	cmp    $0x5e,%eax
f0100fc8:	76 10                	jbe    f0100fda <vprintfmt+0x217>
						putch('?', putdat);		// 不可见字符
f0100fca:	83 ec 08             	sub    $0x8,%esp
f0100fcd:	ff 75 0c             	pushl  0xc(%ebp)
f0100fd0:	6a 3f                	push   $0x3f
f0100fd2:	ff 55 08             	call   *0x8(%ebp)
f0100fd5:	83 c4 10             	add    $0x10,%esp
f0100fd8:	eb 0d                	jmp    f0100fe7 <vprintfmt+0x224>
					else
						putch(ch, putdat);		// 输出一个字符
f0100fda:	83 ec 08             	sub    $0x8,%esp
f0100fdd:	ff 75 0c             	pushl  0xc(%ebp)
f0100fe0:	52                   	push   %edx
f0100fe1:	ff 55 08             	call   *0x8(%ebp)
f0100fe4:	83 c4 10             	add    $0x10,%esp
					p = "(null)";
				if(width > 0 && padc != '-'){
					for(width -= strnlen(p, precision); width > 0; width--)
						putch(padc, putdat);
				}
				for(; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--){
f0100fe7:	83 eb 01             	sub    $0x1,%ebx
f0100fea:	eb 1a                	jmp    f0101006 <vprintfmt+0x243>
f0100fec:	89 75 08             	mov    %esi,0x8(%ebp)
f0100fef:	8b 75 d0             	mov    -0x30(%ebp),%esi
f0100ff2:	89 5d 0c             	mov    %ebx,0xc(%ebp)
f0100ff5:	8b 5d e0             	mov    -0x20(%ebp),%ebx
f0100ff8:	eb 0c                	jmp    f0101006 <vprintfmt+0x243>
f0100ffa:	89 75 08             	mov    %esi,0x8(%ebp)
f0100ffd:	8b 75 d0             	mov    -0x30(%ebp),%esi
f0101000:	89 5d 0c             	mov    %ebx,0xc(%ebp)
f0101003:	8b 5d e0             	mov    -0x20(%ebp),%ebx
f0101006:	83 c7 01             	add    $0x1,%edi
f0101009:	0f b6 47 ff          	movzbl -0x1(%edi),%eax
f010100d:	0f be d0             	movsbl %al,%edx
f0101010:	85 d2                	test   %edx,%edx
f0101012:	74 23                	je     f0101037 <vprintfmt+0x274>
f0101014:	85 f6                	test   %esi,%esi
f0101016:	78 a1                	js     f0100fb9 <vprintfmt+0x1f6>
f0101018:	83 ee 01             	sub    $0x1,%esi
f010101b:	79 9c                	jns    f0100fb9 <vprintfmt+0x1f6>
f010101d:	89 df                	mov    %ebx,%edi
f010101f:	8b 75 08             	mov    0x8(%ebp),%esi
f0101022:	8b 5d 0c             	mov    0xc(%ebp),%ebx
f0101025:	eb 18                	jmp    f010103f <vprintfmt+0x27c>
						putch('?', putdat);		// 不可见字符
					else
						putch(ch, putdat);		// 输出一个字符
				}
				for(; width > 0; width--)			// 用空格填充宽度
					putch(' ', putdat);
f0101027:	83 ec 08             	sub    $0x8,%esp
f010102a:	53                   	push   %ebx
f010102b:	6a 20                	push   $0x20
f010102d:	ff d6                	call   *%esi
					if(altflag && (ch < ' ' || ch > '~'))
						putch('?', putdat);		// 不可见字符
					else
						putch(ch, putdat);		// 输出一个字符
				}
				for(; width > 0; width--)			// 用空格填充宽度
f010102f:	83 ef 01             	sub    $0x1,%edi
f0101032:	83 c4 10             	add    $0x10,%esp
f0101035:	eb 08                	jmp    f010103f <vprintfmt+0x27c>
f0101037:	89 df                	mov    %ebx,%edi
f0101039:	8b 75 08             	mov    0x8(%ebp),%esi
f010103c:	8b 5d 0c             	mov    0xc(%ebp),%ebx
f010103f:	85 ff                	test   %edi,%edi
f0101041:	7f e4                	jg     f0101027 <vprintfmt+0x264>
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0101043:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f0101046:	e9 9e fd ff ff       	jmp    f0100de9 <vprintfmt+0x26>
		return va_arg(*ap, unsigned int);
}

// 返回可变参数列表的各种可能的大小，由于符号扩展不能使用 getuint 时使用
static long long getint(va_list *ap, int lflag){
	if(lflag >= 2)
f010104b:	83 fa 01             	cmp    $0x1,%edx
f010104e:	7e 16                	jle    f0101066 <vprintfmt+0x2a3>
		return va_arg(*ap, long long);
f0101050:	8b 45 14             	mov    0x14(%ebp),%eax
f0101053:	8d 50 08             	lea    0x8(%eax),%edx
f0101056:	89 55 14             	mov    %edx,0x14(%ebp)
f0101059:	8b 50 04             	mov    0x4(%eax),%edx
f010105c:	8b 00                	mov    (%eax),%eax
f010105e:	89 45 d8             	mov    %eax,-0x28(%ebp)
f0101061:	89 55 dc             	mov    %edx,-0x24(%ebp)
f0101064:	eb 32                	jmp    f0101098 <vprintfmt+0x2d5>
	else if(lflag)
f0101066:	85 d2                	test   %edx,%edx
f0101068:	74 18                	je     f0101082 <vprintfmt+0x2bf>
		return va_arg(*ap, long);
f010106a:	8b 45 14             	mov    0x14(%ebp),%eax
f010106d:	8d 50 04             	lea    0x4(%eax),%edx
f0101070:	89 55 14             	mov    %edx,0x14(%ebp)
f0101073:	8b 00                	mov    (%eax),%eax
f0101075:	89 45 d8             	mov    %eax,-0x28(%ebp)
f0101078:	89 c1                	mov    %eax,%ecx
f010107a:	c1 f9 1f             	sar    $0x1f,%ecx
f010107d:	89 4d dc             	mov    %ecx,-0x24(%ebp)
f0101080:	eb 16                	jmp    f0101098 <vprintfmt+0x2d5>
	else
		return va_arg(*ap, int);
f0101082:	8b 45 14             	mov    0x14(%ebp),%eax
f0101085:	8d 50 04             	lea    0x4(%eax),%edx
f0101088:	89 55 14             	mov    %edx,0x14(%ebp)
f010108b:	8b 00                	mov    (%eax),%eax
f010108d:	89 45 d8             	mov    %eax,-0x28(%ebp)
f0101090:	89 c1                	mov    %eax,%ecx
f0101092:	c1 f9 1f             	sar    $0x1f,%ecx
f0101095:	89 4d dc             	mov    %ecx,-0x24(%ebp)
				for(; width > 0; width--)			// 用空格填充宽度
					putch(' ', putdat);
				break;

			case 'd':					// 输出数字
				num = getint(&ap, lflag);
f0101098:	8b 45 d8             	mov    -0x28(%ebp),%eax
f010109b:	8b 55 dc             	mov    -0x24(%ebp),%edx
				if((long long) num < 0){			// 负数
					putch('-', putdat);
					num = -(long long) num;
				}
				base = 10;
f010109e:	b9 0a 00 00 00       	mov    $0xa,%ecx
					putch(' ', putdat);
				break;

			case 'd':					// 输出数字
				num = getint(&ap, lflag);
				if((long long) num < 0){			// 负数
f01010a3:	83 7d dc 00          	cmpl   $0x0,-0x24(%ebp)
f01010a7:	79 74                	jns    f010111d <vprintfmt+0x35a>
					putch('-', putdat);
f01010a9:	83 ec 08             	sub    $0x8,%esp
f01010ac:	53                   	push   %ebx
f01010ad:	6a 2d                	push   $0x2d
f01010af:	ff d6                	call   *%esi
					num = -(long long) num;
f01010b1:	8b 45 d8             	mov    -0x28(%ebp),%eax
f01010b4:	8b 55 dc             	mov    -0x24(%ebp),%edx
f01010b7:	f7 d8                	neg    %eax
f01010b9:	83 d2 00             	adc    $0x0,%edx
f01010bc:	f7 da                	neg    %edx
f01010be:	83 c4 10             	add    $0x10,%esp
				}
				base = 10;
f01010c1:	b9 0a 00 00 00       	mov    $0xa,%ecx
f01010c6:	eb 55                	jmp    f010111d <vprintfmt+0x35a>
				goto number;

			case 'u':					// 无符号数字
				num = getuint(&ap, lflag);
f01010c8:	8d 45 14             	lea    0x14(%ebp),%eax
f01010cb:	e8 7f fc ff ff       	call   f0100d4f <getuint>
				base = 10;
f01010d0:	b9 0a 00 00 00       	mov    $0xa,%ecx
				goto number;
f01010d5:	eb 46                	jmp    f010111d <vprintfmt+0x35a>

			case 'o':					// 输出八进制数字
				num = getuint(&ap, lflag);
f01010d7:	8d 45 14             	lea    0x14(%ebp),%eax
f01010da:	e8 70 fc ff ff       	call   f0100d4f <getuint>
				base = 8;
f01010df:	b9 08 00 00 00       	mov    $0x8,%ecx
				goto number;
f01010e4:	eb 37                	jmp    f010111d <vprintfmt+0x35a>

			case 'p'	:					// 输出指针
				putch('0', putdat);
f01010e6:	83 ec 08             	sub    $0x8,%esp
f01010e9:	53                   	push   %ebx
f01010ea:	6a 30                	push   $0x30
f01010ec:	ff d6                	call   *%esi
				putch('x', putdat);
f01010ee:	83 c4 08             	add    $0x8,%esp
f01010f1:	53                   	push   %ebx
f01010f2:	6a 78                	push   $0x78
f01010f4:	ff d6                	call   *%esi
				num = (unsigned long long)(uintptr_t) va_arg(ap, void*);
f01010f6:	8b 45 14             	mov    0x14(%ebp),%eax
f01010f9:	8d 50 04             	lea    0x4(%eax),%edx
f01010fc:	89 55 14             	mov    %edx,0x14(%ebp)
f01010ff:	8b 00                	mov    (%eax),%eax
f0101101:	ba 00 00 00 00       	mov    $0x0,%edx
				base = 16;
				goto number;
f0101106:	83 c4 10             	add    $0x10,%esp

			case 'p'	:					// 输出指针
				putch('0', putdat);
				putch('x', putdat);
				num = (unsigned long long)(uintptr_t) va_arg(ap, void*);
				base = 16;
f0101109:	b9 10 00 00 00       	mov    $0x10,%ecx
				goto number;
f010110e:	eb 0d                	jmp    f010111d <vprintfmt+0x35a>

			case 'x':					// 输出十六进制数字
			case 'X':
				num = getuint(&ap, lflag);
f0101110:	8d 45 14             	lea    0x14(%ebp),%eax
f0101113:	e8 37 fc ff ff       	call   f0100d4f <getuint>
				base = 16;
f0101118:	b9 10 00 00 00       	mov    $0x10,%ecx

			number:
				printnum(putch, putdat, num, base, width, padc);
f010111d:	83 ec 0c             	sub    $0xc,%esp
f0101120:	0f be 7d d4          	movsbl -0x2c(%ebp),%edi
f0101124:	57                   	push   %edi
f0101125:	ff 75 e0             	pushl  -0x20(%ebp)
f0101128:	51                   	push   %ecx
f0101129:	52                   	push   %edx
f010112a:	50                   	push   %eax
f010112b:	89 da                	mov    %ebx,%edx
f010112d:	89 f0                	mov    %esi,%eax
f010112f:	e8 71 fb ff ff       	call   f0100ca5 <printnum>
				break;
f0101134:	83 c4 20             	add    $0x20,%esp
f0101137:	8b 7d e4             	mov    -0x1c(%ebp),%edi
f010113a:	e9 aa fc ff ff       	jmp    f0100de9 <vprintfmt+0x26>

			case '%':					// 输出百分号
				putch(ch, putdat);
f010113f:	83 ec 08             	sub    $0x8,%esp
f0101142:	53                   	push   %ebx
f0101143:	51                   	push   %ecx
f0101144:	ff d6                	call   *%esi
				break;
f0101146:	83 c4 10             	add    $0x10,%esp
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
f0101149:	8b 7d e4             	mov    -0x1c(%ebp),%edi
				printnum(putch, putdat, num, base, width, padc);
				break;

			case '%':					// 输出百分号
				putch(ch, putdat);
				break;
f010114c:	e9 98 fc ff ff       	jmp    f0100de9 <vprintfmt+0x26>

			default:						// 无法识别的转义序列，只打印其字面值
				putch('%', putdat);
f0101151:	83 ec 08             	sub    $0x8,%esp
f0101154:	53                   	push   %ebx
f0101155:	6a 25                	push   $0x25
f0101157:	ff d6                	call   *%esi
				for(fmt--; fmt[-1] != '%'; fmt--)
f0101159:	83 c4 10             	add    $0x10,%esp
f010115c:	eb 03                	jmp    f0101161 <vprintfmt+0x39e>
f010115e:	83 ef 01             	sub    $0x1,%edi
f0101161:	80 7f ff 25          	cmpb   $0x25,-0x1(%edi)
f0101165:	75 f7                	jne    f010115e <vprintfmt+0x39b>
f0101167:	e9 7d fc ff ff       	jmp    f0100de9 <vprintfmt+0x26>
					;
				break;
		}
	}
}
f010116c:	8d 65 f4             	lea    -0xc(%ebp),%esp
f010116f:	5b                   	pop    %ebx
f0101170:	5e                   	pop    %esi
f0101171:	5f                   	pop    %edi
f0101172:	5d                   	pop    %ebp
f0101173:	c3                   	ret    

f0101174 <vsnprintf>:
	if(b->buf < b->ebuf)
		*b->buf++ = ch;
}

// 打印可变参数列表 ap 到一个buf中，buf长度为 n
int vsnprintf(char *buf, int n, const char *fmt, va_list ap){
f0101174:	55                   	push   %ebp
f0101175:	89 e5                	mov    %esp,%ebp
f0101177:	83 ec 18             	sub    $0x18,%esp
f010117a:	8b 45 08             	mov    0x8(%ebp),%eax
f010117d:	8b 55 0c             	mov    0xc(%ebp),%edx
	struct sprintbuf b = {buf, buf+n-1, 0};
f0101180:	89 45 ec             	mov    %eax,-0x14(%ebp)
f0101183:	8d 4c 10 ff          	lea    -0x1(%eax,%edx,1),%ecx
f0101187:	89 4d f0             	mov    %ecx,-0x10(%ebp)
f010118a:	c7 45 f4 00 00 00 00 	movl   $0x0,-0xc(%ebp)

	if(buf == NULL || n < 1)
f0101191:	85 c0                	test   %eax,%eax
f0101193:	74 26                	je     f01011bb <vsnprintf+0x47>
f0101195:	85 d2                	test   %edx,%edx
f0101197:	7e 22                	jle    f01011bb <vsnprintf+0x47>
		return -E_INVAL;

	// 打印字符串到 buffer 中
	vprintfmt((void*)sprintputch, &b, fmt, ap);
f0101199:	ff 75 14             	pushl  0x14(%ebp)
f010119c:	ff 75 10             	pushl  0x10(%ebp)
f010119f:	8d 45 ec             	lea    -0x14(%ebp),%eax
f01011a2:	50                   	push   %eax
f01011a3:	68 89 0d 10 f0       	push   $0xf0100d89
f01011a8:	e8 16 fc ff ff       	call   f0100dc3 <vprintfmt>

	// 字符串的结束位置
	*b.buf = '\0';
f01011ad:	8b 45 ec             	mov    -0x14(%ebp),%eax
f01011b0:	c6 00 00             	movb   $0x0,(%eax)

	return b.cnt;
f01011b3:	8b 45 f4             	mov    -0xc(%ebp),%eax
f01011b6:	83 c4 10             	add    $0x10,%esp
f01011b9:	eb 05                	jmp    f01011c0 <vsnprintf+0x4c>
// 打印可变参数列表 ap 到一个buf中，buf长度为 n
int vsnprintf(char *buf, int n, const char *fmt, va_list ap){
	struct sprintbuf b = {buf, buf+n-1, 0};

	if(buf == NULL || n < 1)
		return -E_INVAL;
f01011bb:	b8 fd ff ff ff       	mov    $0xfffffffd,%eax

	// 字符串的结束位置
	*b.buf = '\0';

	return b.cnt;
}
f01011c0:	c9                   	leave  
f01011c1:	c3                   	ret    

f01011c2 <snprintf>:
 * 字符串格式化到一个 buf中，buf长度为 n，返回打印的字符数目
 * buf:		输出缓存
 * n:		缓存长度（最多打印字符数目）
 * fmt:		格式化字符串
 */
int snprintf(char *buf, int n, const char *fmt, ...){
f01011c2:	55                   	push   %ebp
f01011c3:	89 e5                	mov    %esp,%ebp
f01011c5:	83 ec 08             	sub    $0x8,%esp
	va_list ap;
	int rc;

	va_start(ap, fmt);
f01011c8:	8d 45 14             	lea    0x14(%ebp),%eax
	rc = vsnprintf(buf, n ,fmt, ap);
f01011cb:	50                   	push   %eax
f01011cc:	ff 75 10             	pushl  0x10(%ebp)
f01011cf:	ff 75 0c             	pushl  0xc(%ebp)
f01011d2:	ff 75 08             	pushl  0x8(%ebp)
f01011d5:	e8 9a ff ff ff       	call   f0101174 <vsnprintf>
	va_end(ap);

	return rc;
}
f01011da:	c9                   	leave  
f01011db:	c3                   	ret    

f01011dc <readline>:
#define	BUFLEN	1024

static char buf[BUFLEN];

// 输出一段提示，读取一行输入字符串
char* readline(const char *prompt){
f01011dc:	55                   	push   %ebp
f01011dd:	89 e5                	mov    %esp,%ebp
f01011df:	57                   	push   %edi
f01011e0:	56                   	push   %esi
f01011e1:	53                   	push   %ebx
f01011e2:	83 ec 0c             	sub    $0xc,%esp
f01011e5:	8b 45 08             	mov    0x8(%ebp),%eax
	int i, c, echoing;

	if(prompt != NULL)
f01011e8:	85 c0                	test   %eax,%eax
f01011ea:	74 11                	je     f01011fd <readline+0x21>
		cprintf("%s", prompt);
f01011ec:	83 ec 08             	sub    $0x8,%esp
f01011ef:	50                   	push   %eax
f01011f0:	68 7a 1e 10 f0       	push   $0xf0101e7a
f01011f5:	e8 50 f7 ff ff       	call   f010094a <cprintf>
f01011fa:	83 c4 10             	add    $0x10,%esp

	i = 0;
	echoing = iscons(0);
f01011fd:	83 ec 0c             	sub    $0xc,%esp
f0101200:	6a 00                	push   $0x0
f0101202:	e8 4d f4 ff ff       	call   f0100654 <iscons>
f0101207:	89 c7                	mov    %eax,%edi
f0101209:	83 c4 10             	add    $0x10,%esp
	int i, c, echoing;

	if(prompt != NULL)
		cprintf("%s", prompt);

	i = 0;
f010120c:	be 00 00 00 00       	mov    $0x0,%esi
	echoing = iscons(0);

	while(1){
		c = getchar();						// 读入一个字符
f0101211:	e8 2d f4 ff ff       	call   f0100643 <getchar>
f0101216:	89 c3                	mov    %eax,%ebx
		if(c < 0){
f0101218:	85 c0                	test   %eax,%eax
f010121a:	79 18                	jns    f0101234 <readline+0x58>
			cprintf("read error: %e\n", c);
f010121c:	83 ec 08             	sub    $0x8,%esp
f010121f:	50                   	push   %eax
f0101220:	68 5c 20 10 f0       	push   $0xf010205c
f0101225:	e8 20 f7 ff ff       	call   f010094a <cprintf>
			return NULL;
f010122a:	83 c4 10             	add    $0x10,%esp
f010122d:	b8 00 00 00 00       	mov    $0x0,%eax
f0101232:	eb 79                	jmp    f01012ad <readline+0xd1>
		}
		else if((c == '\b' || c == '\x7f') && i > 0){	// 退格和删除
f0101234:	83 f8 7f             	cmp    $0x7f,%eax
f0101237:	0f 94 c2             	sete   %dl
f010123a:	83 f8 08             	cmp    $0x8,%eax
f010123d:	0f 94 c0             	sete   %al
f0101240:	08 c2                	or     %al,%dl
f0101242:	74 1a                	je     f010125e <readline+0x82>
f0101244:	85 f6                	test   %esi,%esi
f0101246:	7e 16                	jle    f010125e <readline+0x82>
			if(echoing)
f0101248:	85 ff                	test   %edi,%edi
f010124a:	74 0d                	je     f0101259 <readline+0x7d>
				cputchar('\b');
f010124c:	83 ec 0c             	sub    $0xc,%esp
f010124f:	6a 08                	push   $0x8
f0101251:	e8 dd f3 ff ff       	call   f0100633 <cputchar>
f0101256:	83 c4 10             	add    $0x10,%esp
			i--;
f0101259:	83 ee 01             	sub    $0x1,%esi
f010125c:	eb b3                	jmp    f0101211 <readline+0x35>
		}
		else if(c >= ' ' && i < BUFLEN - 1){	// 必须可见字符
f010125e:	81 fe fe 03 00 00    	cmp    $0x3fe,%esi
f0101264:	7f 20                	jg     f0101286 <readline+0xaa>
f0101266:	83 fb 1f             	cmp    $0x1f,%ebx
f0101269:	7e 1b                	jle    f0101286 <readline+0xaa>
			if(echoing)
f010126b:	85 ff                	test   %edi,%edi
f010126d:	74 0c                	je     f010127b <readline+0x9f>
				cputchar(c);
f010126f:	83 ec 0c             	sub    $0xc,%esp
f0101272:	53                   	push   %ebx
f0101273:	e8 bb f3 ff ff       	call   f0100633 <cputchar>
f0101278:	83 c4 10             	add    $0x10,%esp
			buf[i++] = c;
f010127b:	88 9e 80 25 11 f0    	mov    %bl,-0xfeeda80(%esi)
f0101281:	8d 76 01             	lea    0x1(%esi),%esi
f0101284:	eb 8b                	jmp    f0101211 <readline+0x35>
		}
		else if(c == '\n' || c == '\r'){		// 读到换行符
f0101286:	83 fb 0d             	cmp    $0xd,%ebx
f0101289:	74 05                	je     f0101290 <readline+0xb4>
f010128b:	83 fb 0a             	cmp    $0xa,%ebx
f010128e:	75 81                	jne    f0101211 <readline+0x35>
			if(echoing)
f0101290:	85 ff                	test   %edi,%edi
f0101292:	74 0d                	je     f01012a1 <readline+0xc5>
				cputchar('\n');
f0101294:	83 ec 0c             	sub    $0xc,%esp
f0101297:	6a 0a                	push   $0xa
f0101299:	e8 95 f3 ff ff       	call   f0100633 <cputchar>
f010129e:	83 c4 10             	add    $0x10,%esp
			buf[i] = 0;
f01012a1:	c6 86 80 25 11 f0 00 	movb   $0x0,-0xfeeda80(%esi)
			return buf;
f01012a8:	b8 80 25 11 f0       	mov    $0xf0112580,%eax
		}
	}
}
f01012ad:	8d 65 f4             	lea    -0xc(%ebp),%esp
f01012b0:	5b                   	pop    %ebx
f01012b1:	5e                   	pop    %esi
f01012b2:	5f                   	pop    %edi
f01012b3:	5d                   	pop    %ebp
f01012b4:	c3                   	ret    

f01012b5 <strlen>:
#define ASM 1

/*
 计算字符串长度
 */
int strlen(const char *s){
f01012b5:	55                   	push   %ebp
f01012b6:	89 e5                	mov    %esp,%ebp
f01012b8:	8b 55 08             	mov    0x8(%ebp),%edx
	int n;
	for(n = 0; *s != '\0'; s++)
f01012bb:	b8 00 00 00 00       	mov    $0x0,%eax
f01012c0:	eb 03                	jmp    f01012c5 <strlen+0x10>
		n++;
f01012c2:	83 c0 01             	add    $0x1,%eax
/*
 计算字符串长度
 */
int strlen(const char *s){
	int n;
	for(n = 0; *s != '\0'; s++)
f01012c5:	80 3c 02 00          	cmpb   $0x0,(%edx,%eax,1)
f01012c9:	75 f7                	jne    f01012c2 <strlen+0xd>
		n++;
	return n;
}
f01012cb:	5d                   	pop    %ebp
f01012cc:	c3                   	ret    

f01012cd <strnlen>:

/*
 计算字符串长度，最多到 size大小
 */
int strnlen(const char *s, size_t size){
f01012cd:	55                   	push   %ebp
f01012ce:	89 e5                	mov    %esp,%ebp
f01012d0:	8b 4d 08             	mov    0x8(%ebp),%ecx
f01012d3:	8b 45 0c             	mov    0xc(%ebp),%eax
	int n;
	for(n = 0; size > 0 && *s != '\0'; s++, size--)
f01012d6:	ba 00 00 00 00       	mov    $0x0,%edx
f01012db:	eb 03                	jmp    f01012e0 <strnlen+0x13>
		n++;
f01012dd:	83 c2 01             	add    $0x1,%edx
/*
 计算字符串长度，最多到 size大小
 */
int strnlen(const char *s, size_t size){
	int n;
	for(n = 0; size > 0 && *s != '\0'; s++, size--)
f01012e0:	39 c2                	cmp    %eax,%edx
f01012e2:	74 08                	je     f01012ec <strnlen+0x1f>
f01012e4:	80 3c 11 00          	cmpb   $0x0,(%ecx,%edx,1)
f01012e8:	75 f3                	jne    f01012dd <strnlen+0x10>
f01012ea:	89 d0                	mov    %edx,%eax
		n++;
	return n;
}
f01012ec:	5d                   	pop    %ebp
f01012ed:	c3                   	ret    

f01012ee <strcpy>:


/*
 字符串拷贝，返回拷贝后字符串首地址，地址重复会发生错误
 */
char *strcpy(char *dst, const char *src){
f01012ee:	55                   	push   %ebp
f01012ef:	89 e5                	mov    %esp,%ebp
f01012f1:	53                   	push   %ebx
f01012f2:	8b 45 08             	mov    0x8(%ebp),%eax
f01012f5:	8b 4d 0c             	mov    0xc(%ebp),%ecx
	char *ret;
	ret = dst;
	while((*dst++ = *src++) != '\0')
f01012f8:	89 c2                	mov    %eax,%edx
f01012fa:	83 c2 01             	add    $0x1,%edx
f01012fd:	83 c1 01             	add    $0x1,%ecx
f0101300:	0f b6 59 ff          	movzbl -0x1(%ecx),%ebx
f0101304:	88 5a ff             	mov    %bl,-0x1(%edx)
f0101307:	84 db                	test   %bl,%bl
f0101309:	75 ef                	jne    f01012fa <strcpy+0xc>
		;
	return ret;
}
f010130b:	5b                   	pop    %ebx
f010130c:	5d                   	pop    %ebp
f010130d:	c3                   	ret    

f010130e <strcat>:

/*
 字符串拼接，返回目的字符串首地址
 */
char *strcat(char *dst, const char *src){
f010130e:	55                   	push   %ebp
f010130f:	89 e5                	mov    %esp,%ebp
f0101311:	53                   	push   %ebx
f0101312:	8b 5d 08             	mov    0x8(%ebp),%ebx
	int len = strlen(dst);
f0101315:	53                   	push   %ebx
f0101316:	e8 9a ff ff ff       	call   f01012b5 <strlen>
f010131b:	83 c4 04             	add    $0x4,%esp
	strcpy(dst + len, src);
f010131e:	ff 75 0c             	pushl  0xc(%ebp)
f0101321:	01 d8                	add    %ebx,%eax
f0101323:	50                   	push   %eax
f0101324:	e8 c5 ff ff ff       	call   f01012ee <strcpy>
	return dst;
}
f0101329:	89 d8                	mov    %ebx,%eax
f010132b:	8b 5d fc             	mov    -0x4(%ebp),%ebx
f010132e:	c9                   	leave  
f010132f:	c3                   	ret    

f0101330 <strncpy>:

/*
 字符串拷贝，最多拷贝 size字节，返回目的字符串首地址
 */
char *strncpy(char *dst, const char *src, size_t size){
f0101330:	55                   	push   %ebp
f0101331:	89 e5                	mov    %esp,%ebp
f0101333:	56                   	push   %esi
f0101334:	53                   	push   %ebx
f0101335:	8b 75 08             	mov    0x8(%ebp),%esi
f0101338:	8b 4d 0c             	mov    0xc(%ebp),%ecx
f010133b:	89 f3                	mov    %esi,%ebx
f010133d:	03 5d 10             	add    0x10(%ebp),%ebx
	size_t i;
	char *ret;
	ret = dst;

	for(i = 0; i < size; i++){
f0101340:	89 f2                	mov    %esi,%edx
f0101342:	eb 0f                	jmp    f0101353 <strncpy+0x23>
		*dst++ = *src;
f0101344:	83 c2 01             	add    $0x1,%edx
f0101347:	0f b6 01             	movzbl (%ecx),%eax
f010134a:	88 42 ff             	mov    %al,-0x1(%edx)
		if(*src != '\0')
			src++;
f010134d:	80 39 01             	cmpb   $0x1,(%ecx)
f0101350:	83 d9 ff             	sbb    $0xffffffff,%ecx
char *strncpy(char *dst, const char *src, size_t size){
	size_t i;
	char *ret;
	ret = dst;

	for(i = 0; i < size; i++){
f0101353:	39 da                	cmp    %ebx,%edx
f0101355:	75 ed                	jne    f0101344 <strncpy+0x14>
		*dst++ = *src;
		if(*src != '\0')
			src++;
	}
	return ret;
}
f0101357:	89 f0                	mov    %esi,%eax
f0101359:	5b                   	pop    %ebx
f010135a:	5e                   	pop    %esi
f010135b:	5d                   	pop    %ebp
f010135c:	c3                   	ret    

f010135d <strlcpy>:

/*
 字符串拷贝，返回拷贝后字符串长度
 */
size_t strlcpy(char *dst, const char *src, size_t size){
f010135d:	55                   	push   %ebp
f010135e:	89 e5                	mov    %esp,%ebp
f0101360:	56                   	push   %esi
f0101361:	53                   	push   %ebx
f0101362:	8b 75 08             	mov    0x8(%ebp),%esi
f0101365:	8b 4d 0c             	mov    0xc(%ebp),%ecx
f0101368:	8b 55 10             	mov    0x10(%ebp),%edx
f010136b:	89 f0                	mov    %esi,%eax
	char *dst_in;
	dst_in = dst;

	if(size > 0){
f010136d:	85 d2                	test   %edx,%edx
f010136f:	74 21                	je     f0101392 <strlcpy+0x35>
f0101371:	8d 44 16 ff          	lea    -0x1(%esi,%edx,1),%eax
f0101375:	89 f2                	mov    %esi,%edx
f0101377:	eb 09                	jmp    f0101382 <strlcpy+0x25>
		while(--size > 0 && *src != '\0')
			*dst++ = *src++;
f0101379:	83 c2 01             	add    $0x1,%edx
f010137c:	83 c1 01             	add    $0x1,%ecx
f010137f:	88 5a ff             	mov    %bl,-0x1(%edx)
size_t strlcpy(char *dst, const char *src, size_t size){
	char *dst_in;
	dst_in = dst;

	if(size > 0){
		while(--size > 0 && *src != '\0')
f0101382:	39 c2                	cmp    %eax,%edx
f0101384:	74 09                	je     f010138f <strlcpy+0x32>
f0101386:	0f b6 19             	movzbl (%ecx),%ebx
f0101389:	84 db                	test   %bl,%bl
f010138b:	75 ec                	jne    f0101379 <strlcpy+0x1c>
f010138d:	89 d0                	mov    %edx,%eax
			*dst++ = *src++;
		*dst = '\0';
f010138f:	c6 00 00             	movb   $0x0,(%eax)
	}
	return dst - dst_in;
f0101392:	29 f0                	sub    %esi,%eax
}
f0101394:	5b                   	pop    %ebx
f0101395:	5e                   	pop    %esi
f0101396:	5d                   	pop    %ebp
f0101397:	c3                   	ret    

f0101398 <strcmp>:


/*
 字符串比较，返回大于0，则第一个字符串大，0则相等
 */
int strcmp(const char *p, const char *q){
f0101398:	55                   	push   %ebp
f0101399:	89 e5                	mov    %esp,%ebp
f010139b:	8b 4d 08             	mov    0x8(%ebp),%ecx
f010139e:	8b 55 0c             	mov    0xc(%ebp),%edx
	while(*p && *p == *q)
f01013a1:	eb 06                	jmp    f01013a9 <strcmp+0x11>
		p++, q++;
f01013a3:	83 c1 01             	add    $0x1,%ecx
f01013a6:	83 c2 01             	add    $0x1,%edx

/*
 字符串比较，返回大于0，则第一个字符串大，0则相等
 */
int strcmp(const char *p, const char *q){
	while(*p && *p == *q)
f01013a9:	0f b6 01             	movzbl (%ecx),%eax
f01013ac:	84 c0                	test   %al,%al
f01013ae:	74 04                	je     f01013b4 <strcmp+0x1c>
f01013b0:	3a 02                	cmp    (%edx),%al
f01013b2:	74 ef                	je     f01013a3 <strcmp+0xb>
		p++, q++;
	return (int)((unsigned char)*p - (unsigned char)*q);
f01013b4:	0f b6 c0             	movzbl %al,%eax
f01013b7:	0f b6 12             	movzbl (%edx),%edx
f01013ba:	29 d0                	sub    %edx,%eax
}
f01013bc:	5d                   	pop    %ebp
f01013bd:	c3                   	ret    

f01013be <strncmp>:

/*
 字符串比较，最多比较 n个字节
 */
int strncmp(const char *p, const char *q, size_t n){
f01013be:	55                   	push   %ebp
f01013bf:	89 e5                	mov    %esp,%ebp
f01013c1:	53                   	push   %ebx
f01013c2:	8b 45 08             	mov    0x8(%ebp),%eax
f01013c5:	8b 55 0c             	mov    0xc(%ebp),%edx
f01013c8:	89 c3                	mov    %eax,%ebx
f01013ca:	03 5d 10             	add    0x10(%ebp),%ebx
	while(n > 0 && *p && *p == *q)
f01013cd:	eb 06                	jmp    f01013d5 <strncmp+0x17>
		n--, p++, q++;
f01013cf:	83 c0 01             	add    $0x1,%eax
f01013d2:	83 c2 01             	add    $0x1,%edx

/*
 字符串比较，最多比较 n个字节
 */
int strncmp(const char *p, const char *q, size_t n){
	while(n > 0 && *p && *p == *q)
f01013d5:	39 d8                	cmp    %ebx,%eax
f01013d7:	74 15                	je     f01013ee <strncmp+0x30>
f01013d9:	0f b6 08             	movzbl (%eax),%ecx
f01013dc:	84 c9                	test   %cl,%cl
f01013de:	74 04                	je     f01013e4 <strncmp+0x26>
f01013e0:	3a 0a                	cmp    (%edx),%cl
f01013e2:	74 eb                	je     f01013cf <strncmp+0x11>
		n--, p++, q++;
	if(n == 0)
		return 0;
	else
		return (int)((unsigned char)*p - (unsigned char)*q);
f01013e4:	0f b6 00             	movzbl (%eax),%eax
f01013e7:	0f b6 12             	movzbl (%edx),%edx
f01013ea:	29 d0                	sub    %edx,%eax
f01013ec:	eb 05                	jmp    f01013f3 <strncmp+0x35>
 */
int strncmp(const char *p, const char *q, size_t n){
	while(n > 0 && *p && *p == *q)
		n--, p++, q++;
	if(n == 0)
		return 0;
f01013ee:	b8 00 00 00 00       	mov    $0x0,%eax
	else
		return (int)((unsigned char)*p - (unsigned char)*q);
}
f01013f3:	5b                   	pop    %ebx
f01013f4:	5d                   	pop    %ebp
f01013f5:	c3                   	ret    

f01013f6 <strchr>:


/*
 字符串字符查找，返回字符第一次出现的地址，没有找到则返回 0
 */
char *strchr(const char *s, char c){
f01013f6:	55                   	push   %ebp
f01013f7:	89 e5                	mov    %esp,%ebp
f01013f9:	8b 45 08             	mov    0x8(%ebp),%eax
f01013fc:	0f b6 4d 0c          	movzbl 0xc(%ebp),%ecx
	for(; *s; s++){
f0101400:	eb 07                	jmp    f0101409 <strchr+0x13>
		if(*s == c)
f0101402:	38 ca                	cmp    %cl,%dl
f0101404:	74 0f                	je     f0101415 <strchr+0x1f>

/*
 字符串字符查找，返回字符第一次出现的地址，没有找到则返回 0
 */
char *strchr(const char *s, char c){
	for(; *s; s++){
f0101406:	83 c0 01             	add    $0x1,%eax
f0101409:	0f b6 10             	movzbl (%eax),%edx
f010140c:	84 d2                	test   %dl,%dl
f010140e:	75 f2                	jne    f0101402 <strchr+0xc>
		if(*s == c)
			return (char *)s;
	}
	return 0;
f0101410:	b8 00 00 00 00       	mov    $0x0,%eax
}
f0101415:	5d                   	pop    %ebp
f0101416:	c3                   	ret    

f0101417 <strfind>:

/*
 字符串查找字符，返回字符第一次出现的地址，没有找到则返回 0(NULL)
 */
char *strfind(const char *s, char c){
f0101417:	55                   	push   %ebp
f0101418:	89 e5                	mov    %esp,%ebp
f010141a:	8b 45 08             	mov    0x8(%ebp),%eax
f010141d:	0f b6 4d 0c          	movzbl 0xc(%ebp),%ecx
	for(; *s; s++)
f0101421:	eb 03                	jmp    f0101426 <strfind+0xf>
f0101423:	83 c0 01             	add    $0x1,%eax
f0101426:	0f b6 10             	movzbl (%eax),%edx
		if(*s == c)
f0101429:	84 d2                	test   %dl,%dl
f010142b:	74 04                	je     f0101431 <strfind+0x1a>
f010142d:	38 ca                	cmp    %cl,%dl
f010142f:	75 f2                	jne    f0101423 <strfind+0xc>
			break;
	return (char *)s;
}
f0101431:	5d                   	pop    %ebp
f0101432:	c3                   	ret    

f0101433 <memset>:
#if ASM

/*
 内存初始化为 c
 */
void *memset(void *v, int c, size_t n){
f0101433:	55                   	push   %ebp
f0101434:	89 e5                	mov    %esp,%ebp
f0101436:	57                   	push   %edi
f0101437:	56                   	push   %esi
f0101438:	53                   	push   %ebx
f0101439:	8b 7d 08             	mov    0x8(%ebp),%edi
f010143c:	8b 4d 10             	mov    0x10(%ebp),%ecx
	char *p;

	if (n == 0)
f010143f:	85 c9                	test   %ecx,%ecx
f0101441:	74 36                	je     f0101479 <memset+0x46>
		return v;
	if ((int)v%4 == 0 && n%4 == 0) {
f0101443:	f7 c7 03 00 00 00    	test   $0x3,%edi
f0101449:	75 28                	jne    f0101473 <memset+0x40>
f010144b:	f6 c1 03             	test   $0x3,%cl
f010144e:	75 23                	jne    f0101473 <memset+0x40>
		c &= 0xFF;
f0101450:	0f b6 55 0c          	movzbl 0xc(%ebp),%edx
		c = (c<<24)|(c<<16)|(c<<8)|c;
f0101454:	89 d3                	mov    %edx,%ebx
f0101456:	c1 e3 08             	shl    $0x8,%ebx
f0101459:	89 d6                	mov    %edx,%esi
f010145b:	c1 e6 18             	shl    $0x18,%esi
f010145e:	89 d0                	mov    %edx,%eax
f0101460:	c1 e0 10             	shl    $0x10,%eax
f0101463:	09 f0                	or     %esi,%eax
f0101465:	09 c2                	or     %eax,%edx
f0101467:	89 d0                	mov    %edx,%eax
f0101469:	09 d8                	or     %ebx,%eax
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n/4)
f010146b:	c1 e9 02             	shr    $0x2,%ecx
	if (n == 0)
		return v;
	if ((int)v%4 == 0 && n%4 == 0) {
		c &= 0xFF;
		c = (c<<24)|(c<<16)|(c<<8)|c;
		asm volatile("cld; rep stosl\n"
f010146e:	fc                   	cld    
f010146f:	f3 ab                	rep stos %eax,%es:(%edi)
f0101471:	eb 06                	jmp    f0101479 <memset+0x46>
			:: "D" (v), "a" (c), "c" (n/4)
			: "cc", "memory");
	} else
		asm volatile("cld; rep stosb\n"
f0101473:	8b 45 0c             	mov    0xc(%ebp),%eax
f0101476:	fc                   	cld    
f0101477:	f3 aa                	rep stos %al,%es:(%edi)
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
	return v;
}
f0101479:	89 f8                	mov    %edi,%eax
f010147b:	5b                   	pop    %ebx
f010147c:	5e                   	pop    %esi
f010147d:	5f                   	pop    %edi
f010147e:	5d                   	pop    %ebp
f010147f:	c3                   	ret    

f0101480 <memmove>:

/*
 内存移动
 */
void *memmove(void *dst, const void *src, size_t n){
f0101480:	55                   	push   %ebp
f0101481:	89 e5                	mov    %esp,%ebp
f0101483:	57                   	push   %edi
f0101484:	56                   	push   %esi
f0101485:	8b 45 08             	mov    0x8(%ebp),%eax
f0101488:	8b 75 0c             	mov    0xc(%ebp),%esi
f010148b:	8b 4d 10             	mov    0x10(%ebp),%ecx
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
f010148e:	39 c6                	cmp    %eax,%esi
f0101490:	73 35                	jae    f01014c7 <memmove+0x47>
f0101492:	8d 14 0e             	lea    (%esi,%ecx,1),%edx
f0101495:	39 d0                	cmp    %edx,%eax
f0101497:	73 2e                	jae    f01014c7 <memmove+0x47>
		s += n;
		d += n;
f0101499:	8d 3c 08             	lea    (%eax,%ecx,1),%edi
f010149c:	89 d6                	mov    %edx,%esi
f010149e:	09 fe                	or     %edi,%esi
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
f01014a0:	f7 c6 03 00 00 00    	test   $0x3,%esi
f01014a6:	75 13                	jne    f01014bb <memmove+0x3b>
f01014a8:	f6 c1 03             	test   $0x3,%cl
f01014ab:	75 0e                	jne    f01014bb <memmove+0x3b>
			asm volatile("std; rep movsl\n"
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
f01014ad:	83 ef 04             	sub    $0x4,%edi
f01014b0:	8d 72 fc             	lea    -0x4(%edx),%esi
f01014b3:	c1 e9 02             	shr    $0x2,%ecx
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("std; rep movsl\n"
f01014b6:	fd                   	std    
f01014b7:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
f01014b9:	eb 09                	jmp    f01014c4 <memmove+0x44>
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
		else
			asm volatile("std; rep movsb\n"
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
f01014bb:	83 ef 01             	sub    $0x1,%edi
f01014be:	8d 72 ff             	lea    -0x1(%edx),%esi
		d += n;
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("std; rep movsl\n"
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
		else
			asm volatile("std; rep movsb\n"
f01014c1:	fd                   	std    
f01014c2:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
		// Some versions of GCC rely on DF being clear
		asm volatile("cld" ::: "cc");
f01014c4:	fc                   	cld    
f01014c5:	eb 1d                	jmp    f01014e4 <memmove+0x64>
f01014c7:	89 f2                	mov    %esi,%edx
f01014c9:	09 c2                	or     %eax,%edx
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
f01014cb:	f6 c2 03             	test   $0x3,%dl
f01014ce:	75 0f                	jne    f01014df <memmove+0x5f>
f01014d0:	f6 c1 03             	test   $0x3,%cl
f01014d3:	75 0a                	jne    f01014df <memmove+0x5f>
			asm volatile("cld; rep movsl\n"
				:: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
f01014d5:	c1 e9 02             	shr    $0x2,%ecx
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
		// Some versions of GCC rely on DF being clear
		asm volatile("cld" ::: "cc");
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("cld; rep movsl\n"
f01014d8:	89 c7                	mov    %eax,%edi
f01014da:	fc                   	cld    
f01014db:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
f01014dd:	eb 05                	jmp    f01014e4 <memmove+0x64>
				:: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
		else
			asm volatile("cld; rep movsb\n"
f01014df:	89 c7                	mov    %eax,%edi
f01014e1:	fc                   	cld    
f01014e2:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
				:: "D" (d), "S" (s), "c" (n) : "cc", "memory");
	}
	return dst;
}
f01014e4:	5e                   	pop    %esi
f01014e5:	5f                   	pop    %edi
f01014e6:	5d                   	pop    %ebp
f01014e7:	c3                   	ret    

f01014e8 <memcpy>:


/*
 内存拷贝，最多拷贝 n个字节
 */
void * memcpy(void *dst, const void *src, size_t n){
f01014e8:	55                   	push   %ebp
f01014e9:	89 e5                	mov    %esp,%ebp
	return memmove(dst, src, n);
f01014eb:	ff 75 10             	pushl  0x10(%ebp)
f01014ee:	ff 75 0c             	pushl  0xc(%ebp)
f01014f1:	ff 75 08             	pushl  0x8(%ebp)
f01014f4:	e8 87 ff ff ff       	call   f0101480 <memmove>
}
f01014f9:	c9                   	leave  
f01014fa:	c3                   	ret    

f01014fb <memcmp>:

/*
 内存大小比较，最多比较 n个字节
 */
int memcmp(const void *v1, const void *v2, size_t n){
f01014fb:	55                   	push   %ebp
f01014fc:	89 e5                	mov    %esp,%ebp
f01014fe:	56                   	push   %esi
f01014ff:	53                   	push   %ebx
f0101500:	8b 45 08             	mov    0x8(%ebp),%eax
f0101503:	8b 55 0c             	mov    0xc(%ebp),%edx
f0101506:	89 c6                	mov    %eax,%esi
f0101508:	03 75 10             	add    0x10(%ebp),%esi
	const uint8_t *s1 = (const uint8_t *)v1;
	const uint8_t *s2 = (const uint8_t *)v2;

	while(n-- > 0){
f010150b:	eb 1a                	jmp    f0101527 <memcmp+0x2c>
		if(*s1 != *s2)
f010150d:	0f b6 08             	movzbl (%eax),%ecx
f0101510:	0f b6 1a             	movzbl (%edx),%ebx
f0101513:	38 d9                	cmp    %bl,%cl
f0101515:	74 0a                	je     f0101521 <memcmp+0x26>
			return (int)*s1 - (int)*s2;
f0101517:	0f b6 c1             	movzbl %cl,%eax
f010151a:	0f b6 db             	movzbl %bl,%ebx
f010151d:	29 d8                	sub    %ebx,%eax
f010151f:	eb 0f                	jmp    f0101530 <memcmp+0x35>
		s1++, s2++;
f0101521:	83 c0 01             	add    $0x1,%eax
f0101524:	83 c2 01             	add    $0x1,%edx
 */
int memcmp(const void *v1, const void *v2, size_t n){
	const uint8_t *s1 = (const uint8_t *)v1;
	const uint8_t *s2 = (const uint8_t *)v2;

	while(n-- > 0){
f0101527:	39 f0                	cmp    %esi,%eax
f0101529:	75 e2                	jne    f010150d <memcmp+0x12>
		if(*s1 != *s2)
			return (int)*s1 - (int)*s2;
		s1++, s2++;
	}
	return 0;
f010152b:	b8 00 00 00 00       	mov    $0x0,%eax
}
f0101530:	5b                   	pop    %ebx
f0101531:	5e                   	pop    %esi
f0101532:	5d                   	pop    %ebp
f0101533:	c3                   	ret    

f0101534 <memfind>:

/*
 内存查找，返回字符 c第一次出现的地址
 */
void *memfind(const void *s, int c, size_t n){
f0101534:	55                   	push   %ebp
f0101535:	89 e5                	mov    %esp,%ebp
f0101537:	8b 45 08             	mov    0x8(%ebp),%eax
f010153a:	8b 4d 0c             	mov    0xc(%ebp),%ecx
	const void *ends = (const char *)s + n;
f010153d:	89 c2                	mov    %eax,%edx
f010153f:	03 55 10             	add    0x10(%ebp),%edx
	for(; s < ends; s++){
f0101542:	eb 07                	jmp    f010154b <memfind+0x17>
		if(*(const unsigned char *)s == (unsigned char)c)
f0101544:	38 08                	cmp    %cl,(%eax)
f0101546:	74 07                	je     f010154f <memfind+0x1b>
/*
 内存查找，返回字符 c第一次出现的地址
 */
void *memfind(const void *s, int c, size_t n){
	const void *ends = (const char *)s + n;
	for(; s < ends; s++){
f0101548:	83 c0 01             	add    $0x1,%eax
f010154b:	39 d0                	cmp    %edx,%eax
f010154d:	72 f5                	jb     f0101544 <memfind+0x10>
		if(*(const unsigned char *)s == (unsigned char)c)
			break;
	}
	return (void *)s;
}
f010154f:	5d                   	pop    %ebp
f0101550:	c3                   	ret    

f0101551 <strtol>:
 * 字符串转数字，返回转换后的数字
 * @param  s      	原字符串
 * @param  endptr 	不能转换的字符串首地址
 * @param  base   	转换进制
 */
long strtol(const char *s, char **endptr, int base){
f0101551:	55                   	push   %ebp
f0101552:	89 e5                	mov    %esp,%ebp
f0101554:	57                   	push   %edi
f0101555:	56                   	push   %esi
f0101556:	53                   	push   %ebx
f0101557:	8b 4d 08             	mov    0x8(%ebp),%ecx
f010155a:	8b 5d 10             	mov    0x10(%ebp),%ebx
	int neg = 0;
	long val = 0;

	while(*s == ' ' || *s == '\t')		// 去掉空白字符
f010155d:	eb 03                	jmp    f0101562 <strtol+0x11>
		s++;
f010155f:	83 c1 01             	add    $0x1,%ecx
 */
long strtol(const char *s, char **endptr, int base){
	int neg = 0;
	long val = 0;

	while(*s == ' ' || *s == '\t')		// 去掉空白字符
f0101562:	0f b6 01             	movzbl (%ecx),%eax
f0101565:	3c 09                	cmp    $0x9,%al
f0101567:	74 f6                	je     f010155f <strtol+0xe>
f0101569:	3c 20                	cmp    $0x20,%al
f010156b:	74 f2                	je     f010155f <strtol+0xe>
		s++;

	if(*s == '+')						// 处理符号
f010156d:	3c 2b                	cmp    $0x2b,%al
f010156f:	75 0a                	jne    f010157b <strtol+0x2a>
		s++;
f0101571:	83 c1 01             	add    $0x1,%ecx
 * @param  s      	原字符串
 * @param  endptr 	不能转换的字符串首地址
 * @param  base   	转换进制
 */
long strtol(const char *s, char **endptr, int base){
	int neg = 0;
f0101574:	bf 00 00 00 00       	mov    $0x0,%edi
f0101579:	eb 10                	jmp    f010158b <strtol+0x3a>
f010157b:	bf 00 00 00 00       	mov    $0x0,%edi
	while(*s == ' ' || *s == '\t')		// 去掉空白字符
		s++;

	if(*s == '+')						// 处理符号
		s++;
	else if(*s == '-')
f0101580:	3c 2d                	cmp    $0x2d,%al
f0101582:	75 07                	jne    f010158b <strtol+0x3a>
		s++, neg = 1;
f0101584:	8d 49 01             	lea    0x1(%ecx),%ecx
f0101587:	66 bf 01 00          	mov    $0x1,%di

	// 处理进制
	if((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
f010158b:	85 db                	test   %ebx,%ebx
f010158d:	0f 94 c0             	sete   %al
f0101590:	f7 c3 ef ff ff ff    	test   $0xffffffef,%ebx
f0101596:	75 19                	jne    f01015b1 <strtol+0x60>
f0101598:	80 39 30             	cmpb   $0x30,(%ecx)
f010159b:	75 14                	jne    f01015b1 <strtol+0x60>
f010159d:	80 79 01 78          	cmpb   $0x78,0x1(%ecx)
f01015a1:	0f 85 8a 00 00 00    	jne    f0101631 <strtol+0xe0>
		s += 2, base = 16;
f01015a7:	83 c1 02             	add    $0x2,%ecx
f01015aa:	bb 10 00 00 00       	mov    $0x10,%ebx
f01015af:	eb 16                	jmp    f01015c7 <strtol+0x76>
	else if(base == 0 && s[0] == '0')
f01015b1:	84 c0                	test   %al,%al
f01015b3:	74 12                	je     f01015c7 <strtol+0x76>
		s++, base = 8;
	else if(base == 0)
		base = 10;
f01015b5:	bb 0a 00 00 00       	mov    $0xa,%ebx
		s++, neg = 1;

	// 处理进制
	if((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
		s += 2, base = 16;
	else if(base == 0 && s[0] == '0')
f01015ba:	80 39 30             	cmpb   $0x30,(%ecx)
f01015bd:	75 08                	jne    f01015c7 <strtol+0x76>
		s++, base = 8;
f01015bf:	83 c1 01             	add    $0x1,%ecx
f01015c2:	bb 08 00 00 00       	mov    $0x8,%ebx
	else if(base == 0)
		base = 10;
f01015c7:	b8 00 00 00 00       	mov    $0x0,%eax
f01015cc:	89 5d 10             	mov    %ebx,0x10(%ebp)

	while(1){						// 开始转换数字
		int dig;
		char ch = *s;
f01015cf:	0f b6 11             	movzbl (%ecx),%edx

		if(ch >= '0' && ch <= '9')
f01015d2:	8d 72 d0             	lea    -0x30(%edx),%esi
f01015d5:	89 f3                	mov    %esi,%ebx
f01015d7:	80 fb 09             	cmp    $0x9,%bl
f01015da:	77 08                	ja     f01015e4 <strtol+0x93>
			dig = ch - '0';
f01015dc:	0f be d2             	movsbl %dl,%edx
f01015df:	83 ea 30             	sub    $0x30,%edx
f01015e2:	eb 22                	jmp    f0101606 <strtol+0xb5>
		else if(ch >= 'a' && ch <= 'f')
f01015e4:	8d 72 9f             	lea    -0x61(%edx),%esi
f01015e7:	89 f3                	mov    %esi,%ebx
f01015e9:	80 fb 05             	cmp    $0x5,%bl
f01015ec:	77 08                	ja     f01015f6 <strtol+0xa5>
			dig = ch - 'a' + 10;
f01015ee:	0f be d2             	movsbl %dl,%edx
f01015f1:	83 ea 57             	sub    $0x57,%edx
f01015f4:	eb 10                	jmp    f0101606 <strtol+0xb5>
		else if(ch >= 'A' && ch <= 'F')
f01015f6:	8d 72 bf             	lea    -0x41(%edx),%esi
f01015f9:	89 f3                	mov    %esi,%ebx
f01015fb:	80 fb 05             	cmp    $0x5,%bl
f01015fe:	77 16                	ja     f0101616 <strtol+0xc5>
			dig = ch - 'A' + 10;
f0101600:	0f be d2             	movsbl %dl,%edx
f0101603:	83 ea 37             	sub    $0x37,%edx
		else
			break;

		if(dig >= base)
f0101606:	3b 55 10             	cmp    0x10(%ebp),%edx
f0101609:	7d 0f                	jge    f010161a <strtol+0xc9>
			break;

		s++, val = (val * base) + dig;
f010160b:	83 c1 01             	add    $0x1,%ecx
f010160e:	0f af 45 10          	imul   0x10(%ebp),%eax
f0101612:	01 d0                	add    %edx,%eax
		// 这里不检测 int溢出
	}
f0101614:	eb b9                	jmp    f01015cf <strtol+0x7e>
f0101616:	89 c2                	mov    %eax,%edx
f0101618:	eb 02                	jmp    f010161c <strtol+0xcb>
f010161a:	89 c2                	mov    %eax,%edx

	if(endptr)
f010161c:	83 7d 0c 00          	cmpl   $0x0,0xc(%ebp)
f0101620:	74 05                	je     f0101627 <strtol+0xd6>
		*endptr = (char *)s;
f0101622:	8b 75 0c             	mov    0xc(%ebp),%esi
f0101625:	89 0e                	mov    %ecx,(%esi)
	return (neg ? -val : val);
f0101627:	85 ff                	test   %edi,%edi
f0101629:	74 0c                	je     f0101637 <strtol+0xe6>
f010162b:	89 d0                	mov    %edx,%eax
f010162d:	f7 d8                	neg    %eax
f010162f:	eb 06                	jmp    f0101637 <strtol+0xe6>
		s++, neg = 1;

	// 处理进制
	if((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
		s += 2, base = 16;
	else if(base == 0 && s[0] == '0')
f0101631:	84 c0                	test   %al,%al
f0101633:	75 8a                	jne    f01015bf <strtol+0x6e>
f0101635:	eb 90                	jmp    f01015c7 <strtol+0x76>
	}

	if(endptr)
		*endptr = (char *)s;
	return (neg ? -val : val);
}
f0101637:	5b                   	pop    %ebx
f0101638:	5e                   	pop    %esi
f0101639:	5f                   	pop    %edi
f010163a:	5d                   	pop    %ebp
f010163b:	c3                   	ret    
f010163c:	66 90                	xchg   %ax,%ax
f010163e:	66 90                	xchg   %ax,%ax

f0101640 <__udivdi3>:
f0101640:	55                   	push   %ebp
f0101641:	57                   	push   %edi
f0101642:	56                   	push   %esi
f0101643:	83 ec 10             	sub    $0x10,%esp
f0101646:	8b 54 24 2c          	mov    0x2c(%esp),%edx
f010164a:	8b 7c 24 20          	mov    0x20(%esp),%edi
f010164e:	8b 74 24 24          	mov    0x24(%esp),%esi
f0101652:	8b 4c 24 28          	mov    0x28(%esp),%ecx
f0101656:	85 d2                	test   %edx,%edx
f0101658:	89 7c 24 04          	mov    %edi,0x4(%esp)
f010165c:	89 34 24             	mov    %esi,(%esp)
f010165f:	89 c8                	mov    %ecx,%eax
f0101661:	75 35                	jne    f0101698 <__udivdi3+0x58>
f0101663:	39 f1                	cmp    %esi,%ecx
f0101665:	0f 87 bd 00 00 00    	ja     f0101728 <__udivdi3+0xe8>
f010166b:	85 c9                	test   %ecx,%ecx
f010166d:	89 cd                	mov    %ecx,%ebp
f010166f:	75 0b                	jne    f010167c <__udivdi3+0x3c>
f0101671:	b8 01 00 00 00       	mov    $0x1,%eax
f0101676:	31 d2                	xor    %edx,%edx
f0101678:	f7 f1                	div    %ecx
f010167a:	89 c5                	mov    %eax,%ebp
f010167c:	89 f0                	mov    %esi,%eax
f010167e:	31 d2                	xor    %edx,%edx
f0101680:	f7 f5                	div    %ebp
f0101682:	89 c6                	mov    %eax,%esi
f0101684:	89 f8                	mov    %edi,%eax
f0101686:	f7 f5                	div    %ebp
f0101688:	89 f2                	mov    %esi,%edx
f010168a:	83 c4 10             	add    $0x10,%esp
f010168d:	5e                   	pop    %esi
f010168e:	5f                   	pop    %edi
f010168f:	5d                   	pop    %ebp
f0101690:	c3                   	ret    
f0101691:	8d b4 26 00 00 00 00 	lea    0x0(%esi,%eiz,1),%esi
f0101698:	3b 14 24             	cmp    (%esp),%edx
f010169b:	77 7b                	ja     f0101718 <__udivdi3+0xd8>
f010169d:	0f bd f2             	bsr    %edx,%esi
f01016a0:	83 f6 1f             	xor    $0x1f,%esi
f01016a3:	0f 84 97 00 00 00    	je     f0101740 <__udivdi3+0x100>
f01016a9:	bd 20 00 00 00       	mov    $0x20,%ebp
f01016ae:	89 d7                	mov    %edx,%edi
f01016b0:	89 f1                	mov    %esi,%ecx
f01016b2:	29 f5                	sub    %esi,%ebp
f01016b4:	d3 e7                	shl    %cl,%edi
f01016b6:	89 c2                	mov    %eax,%edx
f01016b8:	89 e9                	mov    %ebp,%ecx
f01016ba:	d3 ea                	shr    %cl,%edx
f01016bc:	89 f1                	mov    %esi,%ecx
f01016be:	09 fa                	or     %edi,%edx
f01016c0:	8b 3c 24             	mov    (%esp),%edi
f01016c3:	d3 e0                	shl    %cl,%eax
f01016c5:	89 54 24 08          	mov    %edx,0x8(%esp)
f01016c9:	89 e9                	mov    %ebp,%ecx
f01016cb:	89 44 24 0c          	mov    %eax,0xc(%esp)
f01016cf:	8b 44 24 04          	mov    0x4(%esp),%eax
f01016d3:	89 fa                	mov    %edi,%edx
f01016d5:	d3 ea                	shr    %cl,%edx
f01016d7:	89 f1                	mov    %esi,%ecx
f01016d9:	d3 e7                	shl    %cl,%edi
f01016db:	89 e9                	mov    %ebp,%ecx
f01016dd:	d3 e8                	shr    %cl,%eax
f01016df:	09 c7                	or     %eax,%edi
f01016e1:	89 f8                	mov    %edi,%eax
f01016e3:	f7 74 24 08          	divl   0x8(%esp)
f01016e7:	89 d5                	mov    %edx,%ebp
f01016e9:	89 c7                	mov    %eax,%edi
f01016eb:	f7 64 24 0c          	mull   0xc(%esp)
f01016ef:	39 d5                	cmp    %edx,%ebp
f01016f1:	89 14 24             	mov    %edx,(%esp)
f01016f4:	72 11                	jb     f0101707 <__udivdi3+0xc7>
f01016f6:	8b 54 24 04          	mov    0x4(%esp),%edx
f01016fa:	89 f1                	mov    %esi,%ecx
f01016fc:	d3 e2                	shl    %cl,%edx
f01016fe:	39 c2                	cmp    %eax,%edx
f0101700:	73 5e                	jae    f0101760 <__udivdi3+0x120>
f0101702:	3b 2c 24             	cmp    (%esp),%ebp
f0101705:	75 59                	jne    f0101760 <__udivdi3+0x120>
f0101707:	8d 47 ff             	lea    -0x1(%edi),%eax
f010170a:	31 f6                	xor    %esi,%esi
f010170c:	89 f2                	mov    %esi,%edx
f010170e:	83 c4 10             	add    $0x10,%esp
f0101711:	5e                   	pop    %esi
f0101712:	5f                   	pop    %edi
f0101713:	5d                   	pop    %ebp
f0101714:	c3                   	ret    
f0101715:	8d 76 00             	lea    0x0(%esi),%esi
f0101718:	31 f6                	xor    %esi,%esi
f010171a:	31 c0                	xor    %eax,%eax
f010171c:	89 f2                	mov    %esi,%edx
f010171e:	83 c4 10             	add    $0x10,%esp
f0101721:	5e                   	pop    %esi
f0101722:	5f                   	pop    %edi
f0101723:	5d                   	pop    %ebp
f0101724:	c3                   	ret    
f0101725:	8d 76 00             	lea    0x0(%esi),%esi
f0101728:	89 f2                	mov    %esi,%edx
f010172a:	31 f6                	xor    %esi,%esi
f010172c:	89 f8                	mov    %edi,%eax
f010172e:	f7 f1                	div    %ecx
f0101730:	89 f2                	mov    %esi,%edx
f0101732:	83 c4 10             	add    $0x10,%esp
f0101735:	5e                   	pop    %esi
f0101736:	5f                   	pop    %edi
f0101737:	5d                   	pop    %ebp
f0101738:	c3                   	ret    
f0101739:	8d b4 26 00 00 00 00 	lea    0x0(%esi,%eiz,1),%esi
f0101740:	3b 4c 24 04          	cmp    0x4(%esp),%ecx
f0101744:	76 0b                	jbe    f0101751 <__udivdi3+0x111>
f0101746:	31 c0                	xor    %eax,%eax
f0101748:	3b 14 24             	cmp    (%esp),%edx
f010174b:	0f 83 37 ff ff ff    	jae    f0101688 <__udivdi3+0x48>
f0101751:	b8 01 00 00 00       	mov    $0x1,%eax
f0101756:	e9 2d ff ff ff       	jmp    f0101688 <__udivdi3+0x48>
f010175b:	90                   	nop
f010175c:	8d 74 26 00          	lea    0x0(%esi,%eiz,1),%esi
f0101760:	89 f8                	mov    %edi,%eax
f0101762:	31 f6                	xor    %esi,%esi
f0101764:	e9 1f ff ff ff       	jmp    f0101688 <__udivdi3+0x48>
f0101769:	66 90                	xchg   %ax,%ax
f010176b:	66 90                	xchg   %ax,%ax
f010176d:	66 90                	xchg   %ax,%ax
f010176f:	90                   	nop

f0101770 <__umoddi3>:
f0101770:	55                   	push   %ebp
f0101771:	57                   	push   %edi
f0101772:	56                   	push   %esi
f0101773:	83 ec 20             	sub    $0x20,%esp
f0101776:	8b 44 24 34          	mov    0x34(%esp),%eax
f010177a:	8b 4c 24 30          	mov    0x30(%esp),%ecx
f010177e:	8b 7c 24 38          	mov    0x38(%esp),%edi
f0101782:	89 c6                	mov    %eax,%esi
f0101784:	89 44 24 10          	mov    %eax,0x10(%esp)
f0101788:	8b 44 24 3c          	mov    0x3c(%esp),%eax
f010178c:	89 4c 24 1c          	mov    %ecx,0x1c(%esp)
f0101790:	89 7c 24 0c          	mov    %edi,0xc(%esp)
f0101794:	89 4c 24 14          	mov    %ecx,0x14(%esp)
f0101798:	89 74 24 18          	mov    %esi,0x18(%esp)
f010179c:	85 c0                	test   %eax,%eax
f010179e:	89 c2                	mov    %eax,%edx
f01017a0:	75 1e                	jne    f01017c0 <__umoddi3+0x50>
f01017a2:	39 f7                	cmp    %esi,%edi
f01017a4:	76 52                	jbe    f01017f8 <__umoddi3+0x88>
f01017a6:	89 c8                	mov    %ecx,%eax
f01017a8:	89 f2                	mov    %esi,%edx
f01017aa:	f7 f7                	div    %edi
f01017ac:	89 d0                	mov    %edx,%eax
f01017ae:	31 d2                	xor    %edx,%edx
f01017b0:	83 c4 20             	add    $0x20,%esp
f01017b3:	5e                   	pop    %esi
f01017b4:	5f                   	pop    %edi
f01017b5:	5d                   	pop    %ebp
f01017b6:	c3                   	ret    
f01017b7:	89 f6                	mov    %esi,%esi
f01017b9:	8d bc 27 00 00 00 00 	lea    0x0(%edi,%eiz,1),%edi
f01017c0:	39 f0                	cmp    %esi,%eax
f01017c2:	77 5c                	ja     f0101820 <__umoddi3+0xb0>
f01017c4:	0f bd e8             	bsr    %eax,%ebp
f01017c7:	83 f5 1f             	xor    $0x1f,%ebp
f01017ca:	75 64                	jne    f0101830 <__umoddi3+0xc0>
f01017cc:	8b 6c 24 14          	mov    0x14(%esp),%ebp
f01017d0:	39 6c 24 0c          	cmp    %ebp,0xc(%esp)
f01017d4:	0f 86 f6 00 00 00    	jbe    f01018d0 <__umoddi3+0x160>
f01017da:	3b 44 24 18          	cmp    0x18(%esp),%eax
f01017de:	0f 82 ec 00 00 00    	jb     f01018d0 <__umoddi3+0x160>
f01017e4:	8b 44 24 14          	mov    0x14(%esp),%eax
f01017e8:	8b 54 24 18          	mov    0x18(%esp),%edx
f01017ec:	83 c4 20             	add    $0x20,%esp
f01017ef:	5e                   	pop    %esi
f01017f0:	5f                   	pop    %edi
f01017f1:	5d                   	pop    %ebp
f01017f2:	c3                   	ret    
f01017f3:	90                   	nop
f01017f4:	8d 74 26 00          	lea    0x0(%esi,%eiz,1),%esi
f01017f8:	85 ff                	test   %edi,%edi
f01017fa:	89 fd                	mov    %edi,%ebp
f01017fc:	75 0b                	jne    f0101809 <__umoddi3+0x99>
f01017fe:	b8 01 00 00 00       	mov    $0x1,%eax
f0101803:	31 d2                	xor    %edx,%edx
f0101805:	f7 f7                	div    %edi
f0101807:	89 c5                	mov    %eax,%ebp
f0101809:	8b 44 24 10          	mov    0x10(%esp),%eax
f010180d:	31 d2                	xor    %edx,%edx
f010180f:	f7 f5                	div    %ebp
f0101811:	89 c8                	mov    %ecx,%eax
f0101813:	f7 f5                	div    %ebp
f0101815:	eb 95                	jmp    f01017ac <__umoddi3+0x3c>
f0101817:	89 f6                	mov    %esi,%esi
f0101819:	8d bc 27 00 00 00 00 	lea    0x0(%edi,%eiz,1),%edi
f0101820:	89 c8                	mov    %ecx,%eax
f0101822:	89 f2                	mov    %esi,%edx
f0101824:	83 c4 20             	add    $0x20,%esp
f0101827:	5e                   	pop    %esi
f0101828:	5f                   	pop    %edi
f0101829:	5d                   	pop    %ebp
f010182a:	c3                   	ret    
f010182b:	90                   	nop
f010182c:	8d 74 26 00          	lea    0x0(%esi,%eiz,1),%esi
f0101830:	b8 20 00 00 00       	mov    $0x20,%eax
f0101835:	89 e9                	mov    %ebp,%ecx
f0101837:	29 e8                	sub    %ebp,%eax
f0101839:	d3 e2                	shl    %cl,%edx
f010183b:	89 c7                	mov    %eax,%edi
f010183d:	89 44 24 18          	mov    %eax,0x18(%esp)
f0101841:	8b 44 24 0c          	mov    0xc(%esp),%eax
f0101845:	89 f9                	mov    %edi,%ecx
f0101847:	d3 e8                	shr    %cl,%eax
f0101849:	89 c1                	mov    %eax,%ecx
f010184b:	8b 44 24 0c          	mov    0xc(%esp),%eax
f010184f:	09 d1                	or     %edx,%ecx
f0101851:	89 fa                	mov    %edi,%edx
f0101853:	89 4c 24 10          	mov    %ecx,0x10(%esp)
f0101857:	89 e9                	mov    %ebp,%ecx
f0101859:	d3 e0                	shl    %cl,%eax
f010185b:	89 f9                	mov    %edi,%ecx
f010185d:	89 44 24 0c          	mov    %eax,0xc(%esp)
f0101861:	89 f0                	mov    %esi,%eax
f0101863:	d3 e8                	shr    %cl,%eax
f0101865:	89 e9                	mov    %ebp,%ecx
f0101867:	89 c7                	mov    %eax,%edi
f0101869:	8b 44 24 1c          	mov    0x1c(%esp),%eax
f010186d:	d3 e6                	shl    %cl,%esi
f010186f:	89 d1                	mov    %edx,%ecx
f0101871:	89 fa                	mov    %edi,%edx
f0101873:	d3 e8                	shr    %cl,%eax
f0101875:	89 e9                	mov    %ebp,%ecx
f0101877:	09 f0                	or     %esi,%eax
f0101879:	8b 74 24 1c          	mov    0x1c(%esp),%esi
f010187d:	f7 74 24 10          	divl   0x10(%esp)
f0101881:	d3 e6                	shl    %cl,%esi
f0101883:	89 d1                	mov    %edx,%ecx
f0101885:	f7 64 24 0c          	mull   0xc(%esp)
f0101889:	39 d1                	cmp    %edx,%ecx
f010188b:	89 74 24 14          	mov    %esi,0x14(%esp)
f010188f:	89 d7                	mov    %edx,%edi
f0101891:	89 c6                	mov    %eax,%esi
f0101893:	72 0a                	jb     f010189f <__umoddi3+0x12f>
f0101895:	39 44 24 14          	cmp    %eax,0x14(%esp)
f0101899:	73 10                	jae    f01018ab <__umoddi3+0x13b>
f010189b:	39 d1                	cmp    %edx,%ecx
f010189d:	75 0c                	jne    f01018ab <__umoddi3+0x13b>
f010189f:	89 d7                	mov    %edx,%edi
f01018a1:	89 c6                	mov    %eax,%esi
f01018a3:	2b 74 24 0c          	sub    0xc(%esp),%esi
f01018a7:	1b 7c 24 10          	sbb    0x10(%esp),%edi
f01018ab:	89 ca                	mov    %ecx,%edx
f01018ad:	89 e9                	mov    %ebp,%ecx
f01018af:	8b 44 24 14          	mov    0x14(%esp),%eax
f01018b3:	29 f0                	sub    %esi,%eax
f01018b5:	19 fa                	sbb    %edi,%edx
f01018b7:	d3 e8                	shr    %cl,%eax
f01018b9:	0f b6 4c 24 18       	movzbl 0x18(%esp),%ecx
f01018be:	89 d7                	mov    %edx,%edi
f01018c0:	d3 e7                	shl    %cl,%edi
f01018c2:	89 e9                	mov    %ebp,%ecx
f01018c4:	09 f8                	or     %edi,%eax
f01018c6:	d3 ea                	shr    %cl,%edx
f01018c8:	83 c4 20             	add    $0x20,%esp
f01018cb:	5e                   	pop    %esi
f01018cc:	5f                   	pop    %edi
f01018cd:	5d                   	pop    %ebp
f01018ce:	c3                   	ret    
f01018cf:	90                   	nop
f01018d0:	8b 74 24 10          	mov    0x10(%esp),%esi
f01018d4:	29 f9                	sub    %edi,%ecx
f01018d6:	19 c6                	sbb    %eax,%esi
f01018d8:	89 4c 24 14          	mov    %ecx,0x14(%esp)
f01018dc:	89 74 24 18          	mov    %esi,0x18(%esp)
f01018e0:	e9 ff fe ff ff       	jmp    f01017e4 <__umoddi3+0x74>
