## 1. Do you have to do anything else to ensure that this I/O privilege setting is saved and restored properly when you subsequently switch from one environment to another? Why?

不需要，因为 I/O 权限位在 Env.Trapframe.tf_eflags 寄存器上，在进程环境切换时，该寄存器会随着 Trapframe 一起保存到栈上。

