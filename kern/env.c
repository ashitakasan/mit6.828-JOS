#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>
#include <inc/elf.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/monitor.h>

struct Env *envs = NULL;				// 全部的运行环境
struct Env *curenv = NULL;			// 当前运行环境
static struct Env *env_free_list;	// 空闲环境列表； env->env_link

#define ENVGENSHIFT		12			// 大于等于 LOGNENV


// 全局描述符表
// 为内核模式和用户模式设置具有单独段的全局描述符表（GDT），
// 为内核模式和用户模式设置具有单独段的全局描述符表（GDT），
// 我们不使用它们的任何内存映射功能，但是我们需要它们来切换特权级别；
// 
// 除了DPL，内核和用户段是相同的，要装载SS寄存器，CPL必须等于DPL，
// 因此，我们必须为用户和内核复制 段；
// 特别的，在gdt的定义中使用的SEG宏的最后一个参数指定该描述符的描述符特权级别（DPL），内核 0，用户 3

struct Segdesc gdt[] = {
	// 0, 未使用（总是故障 - 用于捕获NULL远指针）
	SEG_NULL,
	// 0x8 内核代码段
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),
	// 0x10 内核数据段
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),
	// 0x18 用户代码段
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),
	// 0x20 用户数据段
	[GD_UD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 3),
	// 0x28 tss，在 trap_init_percpu 中初始化
	[GD_TSS0 >> 3] = SEG_NULL
};

struct Pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (unsigned long)gdt
};


/*
  将 env_id 转换为 env 指针；
  如果设置了 checkperm，指定的环境必须是当前环境或当前环境的直接子级；
  返回值：
  	0 表示成功，设置 *env_store 为 指定的 env
  	-E_BAD_ENV 表示错误，设置 *env_store 为 NULL
 */
int evnid2env(envid_t envid, struct Env **env_store, bool checkperm){
	struct Env *e;

	// 如果 envid 为 0，返回当前的运行环境
	if(envid == 0){
		*env_store = curenv;
		return 0;
	}

	// 通过envid的索引部分查找Env结构，然后检查该Env结构的 env_id 部分，确保其不是旧值
	// 即确保 该Env结构 不是指向上一个使用相同 env 的环境
	e = &envs[ENVX(envid)];
	if(e->env_status == ENV_FREE || e->env_id != envid){
		*env_store = 0;
		return -E_BAD_ENV;
	}

	// 检查调用环境是否具有操作指定环境的合法权限，
	// 如果设置了 checkperm，查找的环境必须是当前环境或当前环境的直接子级
	if(checkperm && e != curenv && e->env_parent_id != curenv->env_id){
		*env_store = 0;
		return -E_BAD_ENV;
	}

	*env_store = e;
	return 0;
}

/*
  将 envs 中所有的运行环境标记为空闲，设置 env_ids 为 0，将它们插入 env_free_list；
  确保环境在空闲列表中的顺序与它们在envs数组中的顺序相同（这样第一次调用 env_alloc() 将返回 envs[0]）
 */
void env_init(void){
	// 初始化 envs 数组
	
	int i = NENV - 1;
	struct Env *env_p;

	for(; i >= 0; i--){
		env_p = &envs[i];
		env_p->env_id = 0;
		env_p->env_link = env_free_list;
		env_free_list = env_p;
	}

	// 每CPU部分初始化
	env_init_percpu();
}

/*
  加载GDT和段描述符
 */
void env_init_percpu(void){
	lgdt(&gdt_pd);
	// 内核从不使用 GS FS，所以我们将这些设置为用户数据段
	asm volatile("movw %%ax,%%gs" : : "a" (GD_UD|3));
	asm volatile("movw %%ax,%%fs" : : "a" (GD_UD|3));
	// 内核也是用 ES DS SS，我们将根据需要在内核和用户数据段之间进行切换
	asm volatile("movw %%ax,%%es" : : "a" (GD_KD));
	asm volatile("movw %%ax,%%ds" : : "a" (GD_KD));
	asm volatile("movw %%ax,%%ss" : : "a" (GD_KD));
	// 将内核文本段加载到CS
	asm volatile("ljmp %0,$1f\n 1:\n" : : "i" (GD_KT));
	// 为了良好的测量，清除本地描述符表(LDT)，因为我们不使用它
	llgt(0);
}

/*
  初始化环境 e 的内核虚拟内存布局。
  分配一个页目录，设置 e->env_pgdir，并初始化新环境的地址空间的内核部分；
  不要将任何内容映射到环境的虚拟地址空间的用户部分；
  成功返回 0，错误返回 小于0；错误包括： -E_NO_MEM，如果无法分配页目录或表
 */
static int env_setup_vm(struct Env *e){
	int i;
	struct PageInfo *p = NULL;

	// 为 页目录 分配一个页面
	if(!(p = page_alloc(ALLOC_ZERO)))
		return -E_NO_MEM;
	
	// 现在，设置 e->env_pgdir 并初始化 页目录
	//  - 所有 envs 的虚拟地址空间 UTOP 以上部分都相同（除了 UVPT），可以使用 kern_pgdir 为模板
	//  - 虚拟地址空间的 UTOP 以下部分都 初始化为 空
	//  - 不需要再调用page_alloc
	//  - 仅对于在 UTOP 上方映射的物理页，不维护pp_ref，需要增加env_pgdir的pp_ref以使env_free工作
	
	e->env_pgdir = page2kva(p);
	p->pp_ref++;
	
	i = PDX(UTOP);
	for(; i < NPDENTRIES; i++){
		if(i == PDX(UVPT))
			continue;
		e->env_pgdir[i] = kern_pgdir[i];
	}

	// UVPT映射env自己的页表为只读，权限：内核只读，用户只读
	e->env_pgdir[PDX(UVPT)] = PADDR(e->env_pgdir) | PTE_P | PTE_U;

	return 0;
}

/*
  分配并初始化一个新的运行环境；如果成功，新的运行环境保存在 *newenv_store 中
  成功返回 0， 失败返回 小于 0；
  错误包括： -E_NO_FREE_ENV 如果所有 NENVS 个运行环境都被分配了，-E_NO_MEM 如果内存耗尽
 */
int env_alloc(struct Env **newenv_store, envid_t parent_id){
	int32_t generation;
	int r;
	struct Env *e;

	if(!(e = env_free_list))
		return -E_NO_FREE_ENV;

	// 分配并设置此环境的页目录
	if((r = env_setup_vm(e)) < 0)
		return r;

	// 为当前环境生成一个 env_id，生成方式为 该环境上一个 env_id + NENV
	generation = (e->env_id + (1 << ENVGENSHIFT)) & ~(NENV - 1);
	if(generation <= 0)					// 不创建负的 env_id
		generation = 1 << ENVGENSHIFT;
	e->env_id = generation | (e - envs);

	// 设置基本状态变量
	e->env_parent_id = parent_id;
	e->env_type = ENV_TYPE_USER;
	e->env_status = ENV_RUNNABLE;
	e->env_runs = 0;

	// 清除所有保存的寄存器状态，以防止这个Env结构中的先前运行环境的寄存器值“泄漏”到新的运行环境中
	memset(&e->env_tf, 0, sizeof(e->env_tf));

	// 为段寄存器设置适当的初始值；
	// GD_UD 是 GDT 中用户数据段的选择器，GD_UT 是 GDT 中用户代码段的选择器；
	// 每个段寄存器的低2位包含请求的特权级别（RPL），3 表示用户模式；
	// 当我们切换特权级别时，硬件执行 涉及存储在描述符本身中的RPL和描述符特权级别（DPL）的各种检查
	e->env_tf.tf_ds = GD_UD | 3;
	e->env_tf.tf_es = GD_UD | 3;
	e->env_tf.tf_ss = GD_UD | 3;
	e->env_tf.tf_esp = USTACKTOP;
	e->env_tf.tf_cs = GD_UT | 3;
	// e->env_tf.tf_eip 稍后修改

	// 提交分配结果
	env_free_list = e->env_link;
	*newenv_store = e;

	cprintf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
	return 0;
}



