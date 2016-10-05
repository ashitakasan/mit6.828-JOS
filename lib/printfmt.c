// 精简的原printf风格的格式化函数
// 由printf的，sprintf的，fprintf中等共同使用
// 也由内核和用户程序共同使用

#include <inc/types.h>
#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/stdarg.h>
#include <inc/error.h>

/**
 * 空间或零填充和字段宽度仅支持数字格式。
 * 特殊格式％e取一个整数错误代码，打印出字符串错误提示，
 * 整数可以是正或负，从而使-E_NO_MEM和E_NO_MEM是等价的。
 */
static const char* const error_string[MAXERROR] = {
	[E_UNSPECIFIED]	= "unspecified error",
	[E_BAD_ENV]	= "bad environment",
	[E_INVAL]	= "invalid parameter",
	[E_NO_MEM]	= "out of memory",
	[E_NO_FREE_ENV]	= "out of environments",
	[E_FAULT]	= "segmentation fault",
};

/**
 * 输出一串数字(小于16进制)，使用指定的路径功能和相关指针putdata
 * putch: 	输出单个字符的函数
 * putdat: 	记录输出字符数目
 * num:		要输出的数字
 * base:		输出进制
 * width:	输出字符宽度
 * padc:		宽度填充
 */
static void printnum(void (*putch)(int, void*), void *putdat, 
	unsigned long long num, unsigned base, int width, int padc){
	if(num >= base)
		printnum(putch, putdat, num / base, base, width-1, padc);
	else{
		while(--width > 0)
			putch(padc, putdat);
	}
	putch("0123456789ABCDEF"[num % base], putdat);
}


// 返回可变参数列表的各种可能的大小，取决于lflag标志参数
static unsigned long long getunit(va_list *ap, int lflag){
	if(lflag >= 2)
		return va_arg(*ap, unsigned long long);
	else if(lflag)
		return va_arg(*ap, unsigned long);
	else
		return va_arg(*ap, unsigned int);
}

// 返回可变参数列表的各种可能的大小，由于符号扩展不能使用 getuint 时使用
static long long getint(va_list *ap, int lflag){
	if(lflag >= 2)
		return va_arg(*ap, long long);
	else if(lflag)
		return va_arg(*ap, long);
	else
		return va_arg(*ap, int);
}


/**
 * 格式化输出函数实现，外部调用的可变参数版本
 * putch: 	输出单个字符的函数
 * putdat: 	记录输出字符数目
 * fmt:		格式化字符串
 */
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...);

/**
 * 输出格式化函数，提取可变参数后的内部实现
 * putch: 	输出单个字符的函数
 * putdat: 	记录输出字符数目
 * fmt:		格式化字符串
 * ap:		可变参数列表指针
 */
void vprintfmt(void (*putch)(int, void*), void *putdat, const char *fmt, va_list ap){
	register const char *p;
	register int ch, err;
	unsigned long long num;				// 保存数字参数
	int base,							// 输出进制
		lflag,							// 参数类型大小
		width,							// 输出宽度控制
		precision,						// 输出精度控制
		altflag;							// 小数点后精度控制
	char padc;							// 宽度填充

	while(1){
		while((ch = *(unsigned char *) fmt++) != '%'){
			if(ch == '\0')
				return;
			putch(ch, putdat);
		}

		// 开始处理 % 格式化语句
		padc = ' ';						// 默认宽度填充为空格
		width = -1;
		precision = -1;
		lflag = 0;
		altflag = 0;

	reswitch:
		switch(ch = *(unsigned char *) fmt++){
			case '-':					// 输出右对齐
				padc = '-';
				goto reswitch;

			case '0':					// 宽度填充为 0
				padc = '0';
				goto reswitch;

			case '1':					// 输出宽度定义
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				for(precision = 0; ; ++fmt){
					precision = precision * 10 + ch - '0';
					ch = *fmt;
					if(ch < '0' || ch > '9')
						break;
				}
				goto process_precision;

			case '*':					// 域宽为*，取函数值为域宽
				precision = va_arg(ap, int);
				goto process_precision;

			case '.':
				if(width < 0)
					width = 0;
				goto reswitch;

			case '#':					// 显示尾部0，表示精度
				altflag = 1;
				goto reswitch;

			process_precision:			// 处理精度
				if(width < 0)
					width = precision, precision = -1;
				goto reswitch;

			case 'l':					// 输出为long long
				lflag++;
				goto reswitch;

			case 'c':					// 输出单个字符
				putch(va_arg(ap, int), putdat);
				break;

			case 'e':					// 输出错误信息
				err = va_arg(ap, int);
				if(err < 0)
					err = -err;
				if(err >= MAXERROR || (p = error_string[err]) == NULL)
					// 无错误描述，直接输出错误代码
					printfmt(putch, putdat, "error %d", err);
				else
					printfmt(putch, putdat, "%s", p);
				break;

			case 's':					// 输出字符串
				if((p = va_arg(ap, char*)) == NULL)
					p = "(null)";
				if(width > 0 && padc != '-'){
					for(width -= strnlen(p, precision); width > 0; width--)
						putch(padc, putdat);
				}
				for(; (ch = *p++) != '\0' && (precision < 0 || --precision >= 0); width--){
					if(altflag && (ch < ' ' || ch > '~'))
						putch('?', putdat);		// 不可见字符
					else
						putch(ch, putdat);		// 输出一个字符
				}
				for(; width > 0; width--)			// 用空格填充宽度
					putch(' ', putdat);
				break;

			case 'd':					// 输出数字
				num = getint(&ap, lflag);
				if((long long) num < 0){			// 负数
					putch('-', putdat);
					num = -(long long) num;
				}
				base = 10;
				goto number;

			case 'u':					// 无符号数字
				num = getuint(&ap, lflag);
				base = 10;
				goto number;

			case 'o':					// 输出八进制数字
				num = getuint(&ap, lflag);
				base = 8;
				goto number;

			case 'p'	:					// 输出指针
				putch('0', putdat);
				putch('x', putdat);
				num = (unsigned long long)(uintptr_t) va_arg(ap, void*);
				base = 16;
				goto number;

			case 'x':					// 输出十六进制数字
			case 'X':
				num = getuint(&ap, lflag);
				base = 16;

			number:
				printnum(putch, putdat, num, base, width, padc);
				break;

			case '%':					// 输出百分号
				putch(ch, putdat);
				break;

			default:						// 无法识别的转义序列，只打印其字面值
				putch('%', putdat);
				for(fmt--; fmt[-1] != '%'; fmt--)
					;
				break;
		}
	}
}

// 格式化输出函数实现，外部调用版本
void printfmt(void (*putch)(int, void*), void *putdat, const char *fmt, ...){
	va_list ap;

	va_start(ap, fmt);
	vprintfmt(putch, putdat, fmt, ap);
	va_end(ap);
}

// 打印字符串到 buf中，buf结束地址为 ebuf，同时用 cnt记录打印的字符数目
struct sprintbuf{
	char *buf;
	char *ebuf;
	int cnt;
};

// 字符打印函数
static void sprintputch(int ch, struct sprintbuf *b){
	b->cnt++;
	if(b->buf < b->ebuf)
		*b->buf++ = ch;
}

// 打印可变参数列表 ap 到一个buf中，buf长度为 n
int vsnprintf(char *buf, int n, const char *fmt, va_list ap){
	struct sprintbuf b = {buf, buf+n-1, 0};

	if(buf == NULL || n < 1)
		return -E_INVAL;

	// 打印字符串到 buffer 中
	vprintfmt((void*)sprintputch, &b, fmt, ap);

	// 字符串的结束位置
	*b.buf = '\0';

	return b.cnt;
}

/**
 * 字符串格式化到一个 buf中，buf长度为 n，返回打印的字符数目
 * buf:		输出缓存
 * n:		缓存长度（最多打印字符数目）
 * fmt:		格式化字符串
 */
int snprintf(char *buf, int n, const char *fmt, ...){
	va_list ap;
	int rc;

	va_start(ap, fmt);
	rc = vsnprintf(buf, n ,fmt, ap);
	va_end(ap);

	return rc;
}
