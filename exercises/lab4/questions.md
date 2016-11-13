## 1. Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? 

```C
code = KADDR(MPENTRY_PADDR);
memmove(code, mpentry_start, mpentry_end - mpentry_start);
```
boot_aps() 将 mpentry.S 加载到了 KERNBASE + MPENTRY_PADDR 上，即虚拟地址上，此时 CPU 已经运行在保护模式下，mpentry.S 中的指令地址都是虚拟地址；在引导 CPU 期间，需要用实际的物理地址来配置寄存器， MPBOOTPHYS 即将 虚拟地址转换为物理地址；
boot.S 不需要这样，因为计算机刚启动时，运行在实模式，所有的指令地址都是实际的物理地址；在运行 mpentry.S 启动其他 CPU 时，必须注意将 虚拟地址 转换为 实际物理地址，才能配置相关寄存器。


## 2. It seems that using the big kernel lock guarantees that only one CPU can run the kernel code at a time. Why do we still need separate kernel stacks for each CPU? Describe a scenario in which using a shared kernel stack will go wrong, even with the protection of the big kernel lock.

当用户代码 发生中断时，将陷入内核处理，执行 trapentry.S，执行过程为：
- 将当前用户进程的 Trapframe 压入 内核栈
- 调用 trap
- trap 判断是否是 用户态的中断，如果是，才获取 大内核锁
- 执行 中断处理程序
很明显，在 trap中，CPU 获取大内核锁 之前，需要将 Trapframe 压入 内核栈；也就是说，如果在 某个CPU执行中断处理程序期间，另一个 CPU 上发生了中断，trapentry.S 会将其 Trapframe 压入同一个内核栈，然后等待 大内核锁；第二个 CPU 将 Trapframe 压入内核栈时，会破坏第一个CPU的 内核栈。


