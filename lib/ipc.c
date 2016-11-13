#include <inc/lib.h>

/*
  
 */
int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store){

	panic("ipc_recv not implemented");
}

/*
  
 */
void ipc_send(envid_t to_env, uint32_t val, void *pg, int perm){

	panic("ipc_send not implemented");
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
