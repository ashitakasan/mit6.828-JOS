### Complete thread_switch.S

```ASM
	.globl thread_switch
thread_switch:
	pushal

	movl	0x8cec, %eax
	movl	%esp, (%eax)
	# movl	current_thread, %eax
	# movl	%esp (%eax)

	movl	0x8cf0, %eax
	movl	(%eax), %esp
	# movl	next_thread, %eax
	# movl	(%eax), %esp

	movl	%eax, 0x8cec
	# movl	%eax, current_thread

	# movl	$0, next_thread

	popal
	ret				/* pop return address from stack */
```

指令都可以用指令下边的注释指令来代替，主要步骤就是：
- 保存当前的 %esp 到 current_thread 的 sp 字段中
- 将 next_thread 地址设置到 %esp 中，以便于线程使用这里的堆栈
- 将 current_thread 指针指向当前的 next_thread
- 由于函数调用引起的 需要保存的寄存器，直接用 pushal popal 即可
