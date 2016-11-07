#include <inc/lib.h>

extern void umain(int argc, char **argv);

const volatile struct Env *thisenv;
const char *binaryname = "<unknown>";

void libmain(int argc, char **argv){
	// 设置 thisenv 指向 envs[] 中当前环境结构
	
	envid_t envid = sys_getenvid();
	
	thisenv = &envs[ENVX(envid)];

	// 保存程序的名称，使panic() 可以使用它
	if(argc > 0)
		binaryname = argv[0];

	umain(argc, argv);
	exit();
}
