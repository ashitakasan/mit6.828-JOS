## 1.Part One: System call tracing
```C
static char *syscall_str[] = {
	[SYS_fork]    "fork",
	[SYS_exit]    "exit",
	[SYS_wait]    "wait",
	[SYS_pipe]    "pipe",
	[SYS_read]    "read",
	[SYS_kill]    "kill",
	[SYS_exec]    "exec",
	[SYS_fstat]   "fstat",
	[SYS_chdir]   "chdir",
	[SYS_dup]     "dup",
	[SYS_getpid]  "getpid",
	[SYS_sbrk]    "sbrk",
	[SYS_sleep]   "sleep",
	[SYS_uptime]  "uptime",
	[SYS_open]    "open",
	[SYS_write]   "write",
	[SYS_mknod]   "mknod",
	[SYS_unlink]  "unlink",
	[SYS_link]    "link",
	[SYS_mkdir]   "mkdir",
	[SYS_close]   "close",
};

void syscall(void){
	int num;

	num = proc->tf->eax;
	if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
		proc->tf->eax = syscalls[num]();
		// num表示数组中的第几个系统调用，proc->tf->eax 中保存了函数调用的返回值
		cprintf("%s -> %d\n", syscall_str[num], proc->tf->eax);
	}
	else {
		cprintf("%d %s: unknown sys call %d\n", proc->pid, proc->name, num);
		proc->tf->eax = -1;
	}
}
```


## 2.Part Two: Date system call

当调用 date(struct rtcdate *r); 函数时，date函数将填充 rtcdate 的内容，date并无实现，他将调用系统调用 sys_date(void)；
当 gcc 编译时，在gcc的链接阶段，date 函数将指向一个汇编指令地址：usys.S 中的 SYSCALL(date)，
当系统调用发生时，SYSCALL(date) 相对于 usys.S 的偏移量（索引）将保存在 eax寄存器中，内核通过该索引在 syscall.c 的 syscalls 数组中查找对应的系统调用函数，系统调用函数的实现在 sysproc.c 中，内核找到 sys_date 函数后开始执行，并且通过 argptr 从用户栈帧获取函数参数。

修改的代码：<br>
date.c
```C
#include "types.h"
#include "date.h"
#include "user.h"

int main(int argc, char **argv){
	struct rtcdate r;

	if (date(&r)) {
		printf(2, "date failed\n");
		exit();
	}
	printf(1, "%d-%d-%d %d:%d:%d UTC\n", r.year, r.month, r.day, r.hour, r.minute, r.second);

	exit();
}
```

user.h
```
...
int date(struct rtcdate *r);
...
```

syscall.h
```C
...
#define SYS_date	   22
```

syscall.c
```C
...
extern int sys_date(void);
static int (*syscalls[])(void) = {
...
[SYS_date]	 sys_date,
};
...
```

sysproc.c
```C
...
// date syscall
int sys_date(void){
	struct rtcdate *r;
	// 通过 argptr 获取date函数参数
	if(argptr(0, (char **)&r, sizeof(struct rtcdate)) < 0)
		return -1;
	cmostime(r);
	return 0;
}
```

usys.S
```ASM
...
SYSCALL(date)
```
