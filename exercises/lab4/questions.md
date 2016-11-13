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


## 3. In your implementation of env_run() you should have called lcr3(). Before and after the call to lcr3(), your code makes references (at least it should) to the variable e, the argument to env_run. Upon loading the %cr3 register, the addressing context used by the MMU is instantly changed. But a virtual address (namely e) has meaning relative to a given address context--the address context specifies the physical address to which the virtual address maps. Why can the pointer e be dereferenced both before and after the addressing switch?

在 env 被创建时，调用 env_setup_vm() 函数：
```C
static int env_setup_vm(struct Env *e){
	int i;
	struct PageInfo *p = NULL;

	// 为 页目录 分配一个页面
	if(!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;

	e->env_pgdir = page2kva(p);
	p->pp_ref++;
	
	i = PDX(UTOP);
	for(; i < NPDENTRIES; i++){
		if(i == PDX(UVPT))
			continue;
		e->env_pgdir[i] = kern_pgdir[i];
	}

	// UVPT映射env自己的页表为只读，权限：内核只读，用户只读
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}
```
`e->env_pgdir[i] = kern_pgdir[i];` 在这里，将内核页目录的 UTOP 以上部分的内存映射，都拷贝到新创建的环境 env 的页目录中； envs 虚拟地址在 虚拟地址 ENVS = UTOP 处，因此用户和内核都能通过页目录访问。


## 4. Whenever the kernel switches from one environment to another, it must ensure the old environment's registers are saved so they can be restored properly later. Why? Where does this happen?

CPU 在任何时刻的运行状态，都是通过寄存器来记录，保存了寄存器即保存了 用户进程的运行状态，就能通过 恢复寄存器值来恢复用户进程运行。<br>
当用户调用 yield() 时，将发起一个系统调用，调用号为 SYS_yield，然后系统通过 int 30 指令陷入内核，执行 trapentry.S，在这里，内核会将当前 Trapframe push 到栈帧中；然后调用陷阱处理函数 trap()，根据中断号执行系统调用，根据系统调用号执行 sys_yield() --> sched_yield()。

