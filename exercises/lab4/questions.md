## 1. Compare kern/mpentry.S side by side with boot/boot.S. Bearing in mind that kern/mpentry.S is compiled and linked to run above KERNBASE just like everything else in the kernel, what is the purpose of macro MPBOOTPHYS? Why is it necessary in kern/mpentry.S but not in boot/boot.S? In other words, what could go wrong if it were omitted in kern/mpentry.S? 

```C
code = KADDR(MPENTRY_PADDR);
memmove(code, mpentry_start, mpentry_end - mpentry_start);
```
boot_aps() 将 mpentry.S 加载到了 KERNBASE + MPENTRY_PADDR 上，即虚拟地址上，此时 CPU 已经运行在保护模式下，mpentry.S 中的指令地址都是虚拟地址；在引导 CPU 期间，需要用实际的物理地址来配置寄存器， MPBOOTPHYS 即将 虚拟地址转换为物理地址；
boot.S 不需要这样，因为计算机刚启动时，运行在实模式，所有的指令地址都是实际的物理地址；在运行 mpentry.S 启动其他 CPU 时，必须注意将 虚拟地址 转换为 实际物理地址，才能配置相关寄存器。

