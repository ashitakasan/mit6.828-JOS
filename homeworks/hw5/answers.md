## xv6 CPU alarm

为 xv6 添加时钟函数，当时钟滴答达到计数值时，执行回调函数。

参考 hw3 为系统添加一个系统调用 syscall，需要修改的文件代码为：
	user.h、syscall.h、syscall.c、sysproc.c、usys.S、trap.c、proc.h、proc.c

其中：
alarmtest.c
```C
#include "types.h"
#include "stat.h"
#include "user.h"

void periodic();

int main(int argc, char *argv[]){
	int i;
	printf(1, "alermtest starting\n");

	alarm(10, periodic);
	for(i = 0; i < 50 * 10000000; i++){
		if((i++ % 10000000) == 0)
			write(2, ".", 1);
	}
	exit();
}

void periodic(){
	printf(1, "alarm!\n");
}
```

sysproc.c
```C
...
// alarm syscall
int sys_alarm(void){
	int ticks;
	void (*handler)();

	if(argint(0, &ticks) < 0)
		return -1;
	if(argptr(1, (char **)&handler, 1) < 0)
		return -1;
	proc->alarmticks = ticks;
	proc->alarmhandler = handler;
	return 0;
}
```

proc.h
```C
...
struct proc {
	...
	int alarmticks;
	int countticks;
	void (*alarmhandler)();
};
...
```

proc.c
```C
...
static struct proc* allocproc(void){
	...
	p->countticks = 0;
	return p;
}
...
```

trap.c
```C
...
case T_IRQ0 + IRQ_TIMER:
	if(cpunum() == 0){
		acquire(&tickslock);
		ticks++;
		wakeup(&ticks);
		release(&tickslock);

		if(proc && (tf->cs & 3) == 3){
			proc->countticks++;
			if(proc->alarmticks == proc->countticks){
				// 修改 EIP寄存器的值，来执行回调函数，同时要保存当前执行的指令地址
				tf->esp -= 4;
				*(uint *)(tf->esp) = tf->eip;
				tf->eip = (uint)proc->alarmhandler;
				proc->countticks = 0;
			}
		}
	}
...
```
