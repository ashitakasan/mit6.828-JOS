// 简单的命令行内核监视器，对控制内核和交互式探索系统有用

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE 80	// VGA 命令行长度

struct Command {
	const char *name;
	const char *desc;
	// 返回-1强制监视器退出
	int (*func)(int argc, char **argv, struct Trapframe *tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
};

/*****  基本内核监控命令的实现  *****/

int mon_help(int argc, char **argv, struct Trapframe *tf){
	int i;

	for(i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

/*
 kerninfo 帮助命令的实现
 */
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf){
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

/*
 backtrace 命令的实现，调试信息，打印全部栈帧信息
 */
int mon_backtrace(int argc, char **argv, struct Trapframe *tf){
	uint32_t ebp = read_ebp();
	struct Eipdebuginfo info;

	while(ebp != 0){
		// eip 为调用者的返回地址，ebp当前位置的下一个 int
		uint32_t eip = *(uint32_t *)(ebp + sizeof(uint32_t));
		cprintf("ebp %08x eip %08x args", ebp, eip);
		int i = 0;
		while(i < 5){
			// 当前ebp向上两个int即为上一栈帧的esp，esp向上即为当前函数栈帧的参数
			cprintf(" %08x", *(uint32_t *)(ebp + (2 + i++) * sizeof(uint32_t)));
		}
		cprintf("\n");

		debuginfo_eip(eip, &info);
		cprintf("\t%s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen,
			info.eip_fn_name, eip - info.eip_fn_addr);
		ebp = *(uint32_t *)(ebp);
	}
	return 0;
}


/*****  内核监控命令解释器  *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int runcmd(char *buf, struct Trapframe *tf){
	int argc;
	char *argv[MAXARGS];
	int i;

	// 将解析命令缓冲区变成空格分隔参数
	argc = 0;
	argv[argc] = 0;				// 初始化 argv
	while(1){
		while(*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if(*buf == 0)
			break;

		if(argc == MAXARGS - 1){
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while(*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	if(argc == 0)
		return 0;

	for(i = 0; i < ARRAY_SIZE(commands); i++){
		if(strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

/*
 交互式命令实现
 */
void monitor(struct Trapframe *tf){
	char *buf;

	cprintf("Welcome to the MIT6.828-JOS kernel monirot!\n");
	cprintf("Type 'help' for a list of commands.\n");

	while(1){
		buf = readline("K> ");
		if(buf != NULL){
			if(runcmd(buf, tf) < 0)
				break;
		}
	}
}
