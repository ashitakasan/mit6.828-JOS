// 互斥自旋锁

#include <inc/types.h>
#include <inc/assert.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <inc/string.h>

#include <kern/cpu.h>
#include <kern/spinlock.h>
#include <kern/kdebug.h>

struct spinlock kern_lock = {
#ifdef DEBUG_SPINLOCK
	.name = "kernel_lock"
#endif
};

#ifdef DEBUG_SPINLOCK
// 通过跟踪％ebp链在pcs []中记录当前调用堆栈
static void get_caller_pcs(uint32_t pcs[]){
	uint32_t *ebp;
	int i;

	ebp = (uint32_t *)read_ebp();
	for(i = 0; i < 10; i++){
		if(ebp == 0 || ebp < (uint32_t *)ULIM)
			break;
		pcs[i] = ebp[1];
		ebp = (uint32_t *)ebp[0];
	}
	for(; i < 10; i++)
		pcs[i] = 0;
}

/*
  检查此CPU是否保持有 该锁
 */
static int holding(struct spinlock *lock){
	return lock->locked && lock->cpu == thiscpu;
}

#endif

void __spin_initlock(struct spinlock *lk, char *name){
	lk->locked = 0;
#ifdef DEBUG_SPINLOCK
	lk->name = name;
	lk->cpu = 0;
#endif
}

/*
  获取锁； 循环（旋转），直到获得锁定；
  长时间保持锁可能会导致其他CPU浪费时间旋转来获取它
 */
void spin_lock(struct spinlock *lk){
#ifdef DEBUG_SPINLOCK
	if(holding(lk))
		panic("CPU %d cannot acquire %s: already holding", cpunum(), lk->name);
#endif

	// xchg 是原子性的
	// 它也序列化，使得获取后的读取不会在它之前排序
	while(xchg(&lk->locked , 1) != 0)
		asm volatile("pause");

#ifdef DEBUG_SPINLOCK
	lk->cpu = thiscpu;
	get_caller_pcs(lk->pcs);
#endif
}

/*
  释放一个锁
 */
void spin_unlock(struct spinlock *lk){
#ifdef DEBUG_SPINLOCK
	if(!holding(lk)){					// 错误
		int i;
		uint32_t pcs[10];
		// Nab在被释放前 获得 EIP 链
		memmove(pcs, lk->pcs, sizeof pcs);
		cprintf("CPU %d cannot release %s: held by CPU %d\nAcquired at:",
				cpunum(), lk->name, lk->cpu->cpu_id);

		for(i = 0; i < 10 && pcs[i]; i++){
			struct Eipdebufinfo info;
			if(debuginfo_eip(pcs[i], &info) >= 0)
				cprintf("  %08x %s:%d: %.*s+%x\n", pcs[i], info.eip_file, 
						info.eip_line, info.eip_fn_namelen, 
						info.eip_fn_name, pcs[i] - info.eip_fn_addr);
			else
				cprintf("  %08x\n", pcs[i]);
		}
		panic("spin_unlock");
	}
	lk->pcs[0] = 0;
	lk->cpu = 0;
#endif

	// xchg 指令相对于引用相同存储器的任何其它指令是原子的（即使用“锁定”前缀）
	// x86 CPU 不会跨加锁指令重新排序加载/存储
	// 因为 xchg() 是使用 asm volatile 实现的，gcc不会在 xchg 上重新排序C语句
	xchg(&lk->locked, 0);
}
