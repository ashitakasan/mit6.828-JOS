#ifndef JOS_KERN_ENV_H
#define JOS_KERN_ENV_H

#include <inc/env.h>

extern struct Env *envs;			// 全部的 env变量
extern struct Env *curenv;		// 当前使用的 env 变量
extern struct Segdesc gdt[];

void	env_init(void);
void 	env_init_percpu(void);
int		env_alloc(struct Env **e, envid_t parent_id);
void	env_free(struct Env *e);
void	env_create(uint8_t *binary, enum EnvType type);
void	env_destroy(struct Env *e);		// 如果 e == curenv 则不会返回

int		envid2env(envid_t envid, struct Env **env_stror, bool checkperm);
// 以下两个函数不会返回
void	env_run(struct Env *e) __attribute__((noreturn));
void	env_pop_tf(struct Trapframe *tf) __attribute__((noreturn));

// 没有这个额外的宏，我们不能传递像TEST这样的宏到ENV_CREATE，因为C预处理器参数预扫描规则的限制
#define ENV_PASTE3(x, y, z)	x ## y ## z

#define ENV_CREATE(x, type)									\
	do{														\
		extern uint8_t ENV_PASTE3(_binary_obj_, x, _start)[];	\
		env_create(ENV_PASTE3(_binary_obj_, x, _start), type);	\
	}while(0)

#endif // !JOS_KERN_ENV_H
