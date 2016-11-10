#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/kbdreg.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/console.h>
#include <kern/trap.h>
#include <kern/picirq.h>

static void cons_intr(int (*proc)(void));
static void cons_putc(int c);

/*
 由于历史的PC设计缺陷必要愚蠢的I/O延时子程序
 */
static void delay(void){
	inb(0x84);
	inb(0x84);
	inb(0x84);
	inb(0x84);
}

/*****  I/O 控制代码  *****/

#define  COM1		0x3F8

#define  COM_RX		0		// 输入:	接收缓冲区（DLAB=0）
#define  COM_TX		0		// 输出: 发送缓冲器（DLAB= 0）
#define  COM_DLL		0		// 输出: 除数锁存器低字节（DLAB=1）
#define  COM_DLM		1		// 输出: 除数锁存器高字节（DLAB=1）
#define  COM_IER		1		// 输出: 中断使能寄存器
#define  COM_IER_RDI	0x01		//   启用接收数据中断
#define  COM_IIR		2		// 输入:	中断ID寄存器
#define  COM_FCR		2		// 输出: FIFO控制寄存器
#define  COM_LCR		3		// 输出: 线控制寄存器
#define  COM_LCR_DLAB	0x80	//   除数锁存访问位
#define  COM_LCR_WLEN8	0x03	//   字长: 8 bits
#define  COM_MCR		4		// 输出: 调制解调器控制寄存器
#define  COM_MCR_RTS	0x02		// RTS 辅助
#define  COM_MCR_DTR	0x01		// DTR 辅助
#define  COM_MCR_OUT2	0x08	// Out2 辅助
#define  COM_LSR		5		// 输入:	线状态寄存器
#define  COM_LSR_DATA	0x01	//   有数据
#define  COM_LSR_TXRDY	0x20	//   发送缓冲区可用
#define  COM_LSR_TSRE	0x40	//   传输器关闭

static bool serial_exists;


/*
 从串口获取数据
 */
static int serial_proc_data(void){
	if(!(inb(COM1 + COM_LSR) & COM_LSR_DATA))
		return -1;
	return inb(COM1 + COM_RX);
}

/*
 串口中断
 */
void serial_intr(void){
	if(serial_exists)
		cons_intr(serial_proc_data);
}

/*
 串口输出字符
 */
static void serial_putc(int c){
	int i;
	for(i = 0; !(inb(COM1 + COM_LSR) & COM_LSR_TXRDY) && i < 12800; i++)
		delay();
	outb(COM1 + COM_TX, c);
}

/*
 串口初始化
 */
static void serial_init(void){
	// 关闭 FIFO
	outb(COM1 + COM_FCR, 0);

	// 设定速率，需要DLAB锁
	outb(COM1 + COM_LCR, COM_LCR_DLAB);
	outb(COM1 + COM_DLL, (uint8_t) (115200 / 9600));
	outb(COM1 + COM_DLM, 0);

	// 8个数据位，1个停止位，奇偶校验位关闭，关闭DLAB锁
	outb(COM1 + COM_LCR, COM_LCR_WLEN8 & ~COM_LCR_DLAB);

	// 没有调制解调器控制
	outb(COM1 + COM_MCR, 0);
	// 启用RCV中断
	outb(COM1 + COM_IER, COM_IER_RDI);

	// 清除任何现有的溢出指示和中断
	// 如果COM_LSR返回值为0xFF说明串行端口不存在
	serial_exists = (inb(COM1 + COM_LSR) != 0xFF);
	(void) inb(COM1 + COM_IIR);
	(void) inb(COM1 + COM_RX);
}


/*****  并口输出代码  ******/

/*
 并口输出一个字符
 */
static void lpt_putc(int c){
	int i;

	for (i = 0; !(inb(0x378+1) & 0x80) && i < 12800; i++)
		delay();

	outb(0x378+0, c);
	outb(0x378+2, 0x08|0x04|0x01);
	outb(0x378+2, 0x08);
}


/*****  文本模式CGA/ VGA显示输出  ******/

static unsigned addr_6845;
static uint16_t *crt_buf;
static uint16_t crt_pos;

/*
 CGA 初始化
 */
static void cga_init(void){
	volatile uint16_t *cp;
	uint16_t was;
	unsigned pos;

	cp = (uint16_t*) (KERNBASE + CGA_BUF);
	was = *cp;
	*cp = (uint16_t) 0xA55A;
	if (*cp != 0xA55A) {
		cp = (uint16_t *) (KERNBASE + MONO_BUF);
		addr_6845 = MONO_BASE;
	} else {
		*cp = was;
		addr_6845 = CGA_BASE;
	}

	/* 提取光标位置 */
	outb(addr_6845, 14);
	pos = inb(addr_6845 + 1) << 8;
	outb(addr_6845, 15);
	pos |= inb(addr_6845 + 1);

	crt_buf = (uint16_t*) cp;
	crt_pos = pos;
}

/*
 CGA 输出一个字符
 */
static void cga_putc(int c){
	// 如果没有属性给出，就用黑白色
	if(!(c & ~0xFF))
		c |= 0x0700;

	switch(c & 0xFF){
		case '\b':				// 退格键
			if(crt_pos > 0){
				crt_pos--;
				crt_buf[crt_pos] = (c & ~0xFF) | ' ';
			}
			break;

		case '\n':				// 换行
			crt_pos += CRT_COLS;

		case '\r':
			crt_pos -= (crt_pos % CRT_COLS);
			break;

		case '\t':
			cons_putc(' ');
			cons_putc(' ');
			cons_putc(' ');
			cons_putc(' ');
			break;

		default:
			crt_buf[crt_pos++] = c;
			break;
	}

	if(crt_pos >= CRT_SIZE){
		int i;
		memmove(crt_buf, crt_buf + CRT_COLS, (CRT_SIZE - CRT_COLS) * sizeof(uint16_t));
		for(i = CRT_SIZE - CRT_COLS; i < CRT_SIZE; i++)
			crt_buf[i] = 0x0700 | ' ';
		crt_pos -= CRT_COLS;
	}

	// 移动光标
	outb(addr_6845, 14);
	outb(addr_6845 + 1, crt_pos >> 8);
	outb(addr_6845, 15);
	outb(addr_6845 + 1, crt_pos);
}


/*****  键盘输入  *****/

#define		NO			0

#define		SHIFT		(1<<0)
#define		CTL			(1<<1)
#define		ALT			(1<<2)

#define		CAPSLOCK	(1<<3)
#define		NUMLOCK		(1<<4)
#define		SCROLLLOCK	(1<<5)

#define		E0ESC		(1<<6)

// 功能键
static uint8_t shiftcode[256] =
{
	[0x1D] = CTL,
	[0x2A] = SHIFT,
	[0x36] = SHIFT,
	[0x38] = ALT,
	[0x9D] = CTL,
	[0xB8] = ALT
};

// 锁键
static uint8_t togglecode[256] =
{
	[0x3A] = CAPSLOCK,
	[0x45] = NUMLOCK,
	[0x46] = SCROLLLOCK
};

// 普通键
static uint8_t normalmap[256] =
{
	NO,   0x1B, '1',  '2',  '3',  '4',  '5',  '6',	// 0x00
	'7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
	'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',	// 0x10
	'o',  'p',  '[',  ']',  '\n', NO,   'a',  's',
	'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',	// 0x20
	'\'', '`',  NO,   '\\', 'z',  'x',  'c',  'v',
	'b',  'n',  'm',  ',',  '.',  '/',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,		[0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,	[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,		[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,			[0xCF] = KEY_END,
	[0xD0] = KEY_DN,			[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,			[0xD3] = KEY_DEL
};

// Shift + 普通键
static uint8_t shiftmap[256] =
{
	NO,   033,  '!',  '@',  '#',  '$',  '%',  '^',	// 0x00
	'&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
	'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',	// 0x10
	'O',  'P',  '{',  '}',  '\n', NO,   'A',  'S',
	'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',	// 0x20
	'"',  '~',  NO,   '|',  'Z',  'X',  'C',  'V',
	'B',  'N',  'M',  '<',  '>',  '?',  NO,   '*',	// 0x30
	NO,   ' ',  NO,   NO,   NO,   NO,   NO,   NO,
	NO,   NO,   NO,   NO,   NO,   NO,   NO,   '7',	// 0x40
	'8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
	'2',  '3',  '0',  '.',  NO,   NO,   NO,   NO,	// 0x50
	[0xC7] = KEY_HOME,		[0x9C] = '\n' /*KP_Enter*/,
	[0xB5] = '/' /*KP_Div*/,	[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,		[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,			[0xCF] = KEY_END,
	[0xD0] = KEY_DN,			[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,			[0xD3] = KEY_DEL
};

#define C(x) (x - '@')

// Ctrl + 普通键
static uint8_t ctlmap[256] =
{
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	NO,      NO,      NO,      NO,      NO,      NO,      NO,      NO,
	C('Q'),  C('W'),  C('E'),  C('R'),  C('T'),  C('Y'),  C('U'),  C('I'),
	C('O'),  C('P'),  NO,      NO,      '\r',    NO,      C('A'),  C('S'),
	C('D'),  C('F'),  C('G'),  C('H'),  C('J'),  C('K'),  C('L'),  NO,
	NO,      NO,      NO,      C('\\'), C('Z'),  C('X'),  C('C'),  C('V'),
	C('B'),  C('N'),  C('M'),  NO,      NO,      C('/'),  NO,      NO,
	[0x97] = KEY_HOME,
	[0xB5] = C('/'),		[0xC8] = KEY_UP,
	[0xC9] = KEY_PGUP,	[0xCB] = KEY_LF,
	[0xCD] = KEY_RT,		[0xCF] = KEY_END,
	[0xD0] = KEY_DN,		[0xD1] = KEY_PGDN,
	[0xD2] = KEY_INS,		[0xD3] = KEY_DEL
};

// 有字符键位
static uint8_t *charcode[4] = {
	normalmap,
	shiftmap,
	ctlmap,
	ctlmap
};


/*
 从键盘获取数据。如果完成一个字符，返回它，否则为0；没有数据则返回-1
 */
static int kbd_proc_data(void){
	int c;
	uint8_t stat, data;
	static uint32_t shift;

	stat = inb(KBSTATP);
	if((stat & KBS_DIB) == 0)
		return -1;
	if(stat & KBS_TERR)			// 忽略鼠标
		return -1;

	data = inb(KBDATAP);

	if (data == 0xE0) {			// E0 表示转义字符
		shift |= E0ESC;
		return 0;
	}
	else if (data & 0x80) {		// 键释放
		data = (shift & E0ESC ? data : data & 0x7F);
		shift &= ~(shiftcode[data] | E0ESC);
		return 0;
	}
	else if (shift & E0ESC) {	// 最后一个字符是一个E0或0x80则逃逸
		data |= 0x80;
		shift &= ~E0ESC;
	}

	shift |= shiftcode[data];
	shift ^= togglecode[data];

	c = charcode[shift & (CTL | SHIFT)][data];
	if (shift & CAPSLOCK) {
		if ('a' <= c && c <= 'z')
			c += 'A' - 'a';
		else if ('A' <= c && c <= 'Z')
			c += 'a' - 'A';
	}

	// 处理特殊键，Ctrl-Alt-Del: 重启
	if (!(~shift & (CTL | ALT)) && c == KEY_DEL) {
		cprintf("Rebooting!\n");
		outb(0x92, 0x3);		// 克里斯·弗罗斯特的礼节
	}
	return c;
}

/*
 键盘中断
 */
void kbd_intr(void){
	cons_intr(kbd_proc_data);
}

/*
 键盘初始化
 */
static void kbd_init(void){
	// 排除kbd缓冲区，使QEMU产生中断
	kdb_intr();
	irq_setmask_8259A(irq_mask_8259A & ~(1 << IRQ_KBD));
}


/*****  一般独立于设备的控制台代码  *****/
// 在这里，我们管理控制台输入缓冲区，
// 每当相应的中断发生时，缓冲区里保存从键盘或串行接收的字符

#define CONSBUFSIZE 512

static struct {
	uint8_t buf[CONSBUFSIZE];
	uint32_t rpos;
	uint32_t wpos;
} cons;


/*
 调用设备中断子程序，输入字符送入循环控制台的输入缓冲区。
 */
static void cons_intr(int (*proc)(void)){
	int c;

	while((c = (*proc)()) != -1){
		if(c == 0)
			continue;
		cons.buf[cons.wpos++] = c;
		if(cons.wpos == CONSBUFSIZE)
			cons.wpos = 0;
	}
}

/*
 返回终端的下一个输入字符，没有则返回 0
 */
int cons_getc(void){
	int c;

	// 轮询任何挂起输入的字符，即使中断被禁止该功能仍然可用
	serial_intr();
	kbd_intr();

	// 获取输入缓冲区的下一个字符
	if(cons.rpos != cons.wpos){
		c = cons.buf[cons.rpos++];
		if(cons.rpos == CONSBUFSIZE)
			cons.rpos = 0;
		return c;
	}
	return 0;
}

/*
 输出一个字符到终端
 */
static void cons_putc(int c){
	serial_putc(c);				// 串口输出
	lpt_putc(c);					// 并口输出
	cga_putc(c);					// CGA输出
}

/*
 终端初始化
 */
void cons_init(void){
	cga_init();					// CGA 初始化
	kbd_init();					// 键盘初始化
	serial_init();				// 串口初始化

	if(!serial_exists)
		cprintf("Serial port does not exist!\n");
}

// 终端I/O，提供给 readline和cprintf 使用

void cputchar(int c){
	cons_putc(c);
}

int getchar(void){
	int c;
	while((c = cons_getc()) == 0)
		;
	return c;
}

int iscons(int fdnum){
	return 1;
}
