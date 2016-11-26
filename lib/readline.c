#include <inc/stdio.h>
#include <inc/error.h>

#define	BUFLEN		1024

static char buf[BUFLEN];

// 输出一段提示，读取一行输入字符串
char* readline(const char *prompt){
	int i, c, echoing;

#if JOS_KERNEL
	if(prompt != NULL)
		cprintf("%s", prompt);
#else
	if(prompt != NULL)
		fprintf(1, "%s\n", prompt);
#endif

	i = 0;
	echoing = iscons(0);

	while(1){
		c = getchar();						// 读入一个字符
		if(c < 0){
			if(c != -E_EOF)
				cprintf("read error: %e\n", c);
			return NULL;
		}
		else if((c == '\b' || c == '\x7f') && i > 0){	// 退格和删除
			if(echoing)
				cputchar('\b');
			i--;
		}
		else if(c >= ' ' && i < BUFLEN - 1){	// 必须可见字符
			if(echoing)
				cputchar(c);
			buf[i++] = c;
		}
		else if(c == '\n' || c == '\r'){		// 读到换行符
			if(echoing)
				cputchar('\n');
			buf[i] = 0;
			return buf;
		}
	}
}
