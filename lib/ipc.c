#include <inc/lib.h>

/*
  通过 IPC 接收值并返回
  如果 'pg' 是非空的，那么发送者发送的任何页面将被映射到该地址；
  如果 'from_env_store' 是非空的，那么将 IPC 发送者的 envid 存储在 *from_env_store；
  如果 'perm_store' 是非空的，那么在 *perm_store 中存储 IPC 发送者的页面权限
  	(如果页面被成功地传送到“pg”，则这是非零的)；
  如果系统调用失败，则将 0 存储在 *fromenv 和 *perm（如果它们是非空的）并返回错误；
  否则，返回发送方发送的值；

  使用 'thisenv' 来查找 value 以及谁发送的；
  如果 'pg' 为 null，传递 sys_ipc_recv 一个值，它会理解为 '没有页面'；
  （零不是正确的值，因为这是一个完美有效的地方来映射临时页面）
 */
int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store){
	// LAB 4
	if(!pg)
		pg = (void *) UTOP;

	int ret;
	if((ret = sys_ipc_recv(pg)) < 0){
		if(from_env_store)
			*from_env_store = 0;
		if(perm_store)
			*perm_store = 0;
	}
	else{
		if(from_env_store)
			*from_env_store = thisenv->env_ipc_from;
		if(perm_store)
			*perm_store = thisenv->env_ipc_perm;

		return thisenv->env_ipc_value;
	}
	return ret;
}

/*
  发送 'val' (和 'pg' 与 'perm'，如果 'pg' 是非空的) 到 'to_env'；
  此功能一直尝试，直到成功； 任何错误都应该 panic 除了 -E_IPC_NOT_RECV；
  使用 sys_yield()
  如果 'pg' 为 null，传递给 sys_ipc_try_send 一个值，它会理解为 无页面。(零不是正确的值)
 */
void ipc_send(envid_t to_env, uint32_t val, void *pg, int perm){
	// LAB 4
	if(!pg)
		pg = (void *) UTOP;

	int ret;
	while(1){
		ret = sys_ipc_try_send(to_env, val, pg, perm);
		if(ret == 0)
			return;
		else if(ret != -E_IPC_NOT_RECV)
			panic("ipc_send: error %e\n", ret);

		sys_yield();
	}
}

/*
  找到给定类型的第一个环境，我们将使用它来寻找特殊的环境。
  如果没有找到 返回 0
 */
envid_t ipc_find_env(enum EnvType type){
	int i;
	for(i = 0; i < NENV; i++)
		if(envs[i].env_type == type)
			return envs[i].env_id;
	return 0;
}
