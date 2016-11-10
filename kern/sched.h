#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

void sched_yield(void) __attribute__((noreturn));

#endif	// !JOS_KERN_SCHED_H
