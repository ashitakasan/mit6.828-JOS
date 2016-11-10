#ifndef JOS_INC_SPINLOCK_H
#define JOS_INC_SPINLOCK_H

#include <inc/types.h>

// 提交此选项以禁用自旋锁调试
#define DEBUG_SPINLOCK

// 互斥锁
struct spinlock {
	unsigned locked;				// 锁在保持中 ?

#ifdef DEBUG_SPINLOCK			// 为了调试
	char *name;					// 锁 名称
	struct CpuInfo *cpu;			// 获取锁的 CPU
	uintptr_t pcs[10];			// 锁定锁的调用堆栈（程序计数器数组）
#endif
};

void __spin_initlock(struct spinlock *lk, char *name);
void spin_lock(struct spinlock *lk);
void spin_unlock(struct spinlock *lk);

#define spin_initlock(lock) __spin_initlock(lock, #lock)

extern struct spinlock kernel_lock;

static inline void lock_kernel(void){
	spin_lock(&kernel_lock);
}

static inline void unlock_kernel(void){
	spin_unlock(&kernel_lock);

	// 通常我们不需要这样做，但QEMU一次只运行一个CPU，并且有很长的时间片
	// 没有暂停，这个CPU很可能在另一个CPU被给予机会获取它之前重新获取块
	asm volatile("pause");
}

#endif
