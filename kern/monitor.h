#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// 激活内核监控，可提供表示当前状态的陷阱栈帧
void monitor(struct Trapframe *tf);

// 实施监控命令功能
int mon_help(int argc, char **argv, struct Trapframe *tf);

int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);

int mon_backtrace(int argc, char **argv, struct Trapframe *tf);

#endif
// !JOS_KERN_MONITOR_H
