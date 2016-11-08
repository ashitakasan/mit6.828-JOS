## 1. Explain in one sentence what happens
```C
struct spinlock lk;
initlock(&lk, "test lock");
acquire(&lk);
acquire(&lk);
```

在第三行，锁将被当前线程获取，并且不会释放（没有释放的代码），因此程序将在第四行阻塞等待锁，由于锁不会释放，程序将在这里停止。


## 2. Explain in a few sentences why the kernel panicked. You may find it useful to look up the stack trace (the sequence of %eip values printed by panic) in the kernel.asm listing.

panic输出：
```TXT
qemu-system-i386 -serial mon:stdio -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 
xv6...
cpu1: starting
cpu0: starting
cpu with apicid 0: panic: sched locks
 80103bd8 80103d52 80105977 801056c8 8010215e 80100183 801013a5 8010149f 801037c4 801056cb%
```
最后一行为 eip 输出，根据 eip 得到栈帧列表信息，函数调用栈如下：
```C
.globl trapret
void forkret(void);
void iinit(int dev);
void readsb(int dev, struct superblock *sb);
struct buf *bread(uint dev, uint blockno);
void iderw(struct buf *b);
.globl _alltraps
void trap(struct trapframe *tf);
void yield(void);
void sched(void);
```

内核在保持锁时，需要关闭中断，否则，如果中断发生在内核保持锁期间，就会导致锁的不一致性，在中断结束后恢复用户进程时，会发现数据变化；
而在该函数中， 获取锁后， sti 开启中断，然后自旋等待（sleep）， cli 关闭中断，然后释放锁。在 iderw 执行期间，正常的用户进程通过 acquire 已经获取了锁资源，此时发生中断，用户进程暂停，锁保持，然后中断处理程序 trap 运行，trap 内部调用 yield ：
```C
void yield(void){
	acquire(&ptable.lock);  //DOC: yieldlock
	proc->state = RUNNABLE;
	sched();
	release(&ptable.lock);
}
```
yield 中，先后去锁，然后调用 sched ：
```C
void sched(void){
	int intena;

	if(!holding(&ptable.lock))
		panic("sched ptable.lock");
	if(cpu->ncli != 1)
		panic("sched locks");
	if(proc->state == RUNNING)
		panic("sched running");
	if(readeflags()&FL_IF)
		panic("sched interruptible");
	intena = cpu->intena;
	swtch(&proc->context, cpu->scheduler);
	cpu->intena = intena;
}
```
第六行， sched 会检查当前锁的数量是不是 1，如果不是 1 就报错 panic，由于系统两次通过 acquire 获取了锁，cpu->ncli = 2，因此这里必然报错。


## 3. Explain in a few sentences why the kernel didn't panic. Why do file_table_lock and ide_lock have different behavior in this respect? <br> You do not need to understand anything about the details of the IDE hardware to answer this question, but you may find it helpful to look at which functions acquire each lock, and then at when those functions get called.

我的程序报错：
```C
qemu-system-i386 -serial mon:stdio -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 
xv6...
cpu1: starting
cpu0: starting
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
cpu with apicid 1: panic: sched locks
 80103bd8 80103d52 80105977 801056c8 8010500b 80104819 80105859 801056c8 0 0
```
最后一行为 eip 输出，根据 eip 得到栈帧列表信息，函数调用栈如下：
```C

.globl _alltraps
void trap(struct trapframe *tf);
voidsyscall(void);
int sys_open(void);
struct file* filealloc(void);
.globl _alltraps
void trap(struct trapframe *tf);
void yield(void);
void sched(void);
```
panic 信息与问题2的 ide_lock 不同， file_table_lock 上启用中断 仍然会发生错误，只不过概率要比 ide_lock 要小的多。


## 4. Why does release() clear lk->pcs[0] and lk->cpu before clearing lk->locked? Why not wait until after?

release() 函数如下：
```C
void release(struct spinlock *lk){
	if(!holding(lk))
		panic("release");

	lk->pcs[0] = 0;
	lk->cpu = 0;

	__sync_synchronize();
	asm volatile("movl $0, %0" : "+m" (lk->locked) : );

	popcli();
}
```

popcli() 将调用 sti() 启用中断，如果先调用 popcli()，那么在执行 lk->pcs[0] = 0 和 lk->cpu = 0 时（或之前），可能会发生中断，
中断会获取锁，会调用 acquire 修改 spinlock 的 pcs[0] 和 cpu ，当中断结束后，程序继续运行，执行这两条语句；此时会修改 中断期间获取的锁的值，导致数据出错，系统报错。
报错信息：
```
qemu-system-i386 -serial mon:stdio -drive file=fs.img,index=1,media=disk,format=raw -drive file=xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 
xv6...
cpu1: starting
cpu0: starting
sb: size 1000 nblocks 941 ninodes 200 nlog 30 logstart 2 inodestart 32 bmap start 58
cpu with apicid 1: panic: release
 801043c8 80103b2d 80102e46 80102e6a 705a 0 0 0 0 0
```
