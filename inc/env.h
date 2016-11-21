#ifndef JOS_INC_ENV_H
#define JOS_INC_ENV_H

#include <inc/types.h>
#include <inc/trap.h>
#include <inc/memlayout.h>

typedef int32_t envid_t;

/*
  运行环境 ID 'envid_t' 由三部分组成：
	+1+---------------21----------------	+--------10------------+
	|0|			Uniqueifier			|	Environment		|
	| |								|		Index		|
	+-----------------------------------	+----------------------+
									 \---- ENVX(eid) ----/
  Environment Index ENVX(eid) 等于其在 envs[] 数组中的偏移量；
  唯一标识符Uniqueifier 区分在不同时间创建的环境，但是共享 Environment Index；
  所有真实环境 ID 都大于0，因此第一个比特位(sign bit)为 0，envid_t 小于 0 表示错误；
  envid_t == 0 是特殊值，代表当前运行环境。
*/

#define LOG2NENV			10
#define NENV				(1 << LOG2NENV)
#define ENVX(envid_t)	((envid_t) & (NENV - 1))

// Env 结构体中 env_status 的值
enum {
	ENV_FREE = 0,				// 表示Env结构处于非活动状态，因此在env_free_list上
	ENV_DYING,					// Env结构表示一个僵尸环境，僵尸环境将在下一次陷入内核时释放
	ENV_RUNNABLE,				// 表示当前运行环境 正在等待在处理器上运行
	ENV_RUNNING,					// 当前运行环境正在运行
	ENV_NOT_RUNNABLE				// 当前运行环境处于活动状态，但没有准备好运行，比如正在等待与其他运行环境的 IPC
};

// 特殊环境类型
enum EnvType {
	ENV_TYPE_USER = 0,
	ENV_TYPE_FS,					// 文件系统服务器
};

// 保存系统运行环境，内核通过该结构体来跟踪 系统每个用户运行环境
struct Env {
	struct Trapframe env_tf;		// 当该Env没有运行时，这里保存运行环境的寄存器值，如内核或其他新的运行环境在运行，
								// 	当进程从用户模式切换到内核模式，内核保存寄存器值，以便恢复当前运行环境；
	struct Env *env_link;		// 链接结构，指向 env_free_list 上的下一个 Env结构；

	envid_t env_id;				// 内核在此存储一个唯一标识当前使用此Env结构的环境的值。当一个用户环境结束时，
								//	内核可能重新分配同一个 Env结构 给一个不同的环境，新的环境会有一个不同的 env_id；
	envid_t env_parent_id;		// 内核在这里保存了创建当前环境的环境的 env_id，这样运行环境之间能形成一个 family tree；
								//  这将有助于做出关于哪些环境被允许做什么对谁的安全决定；
	enum EnvType env_type;		// 这用于区分特殊环境；对于大多数的环境，它等于 ENV_TYPE_USER；

	unsigned env_status;			// 当前环境的运行状态，上边 enum 各项的解释；

	uint32_t env_runs;			// 表示当前运行环境已经运行了多少次；

	int env_cpunum;				// 运行env的CPU

	pde_t *env_pgdir;			// 该变量保存当前运行环境的 页目录的内核虚拟地址，即地址空间

	void *env_pgfault_upcall;	// 页面故障系统调用 入口点

	// IPC
	bool env_ipc_recving;		// Env 被阻止接收
	void *env_ipc_dstva;			// VA 在哪里映射接收的页面
	uint32_t env_ipc_value;		// 进程发送给我们的数据
	envid_t env_ipc_from;		// 发送者的 envid
	int env_ipc_perm;			// 收到页映射的权限
};

#endif // !JOS_INC_ENV_H
