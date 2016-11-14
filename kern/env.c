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
#include <kern/sched.h>
#include <kern/cpu.h>
#include <kern/spinlock.h>

struct Env *envs = NULL;				// 全部的运行环境
// struct Env *curenv = NULL;			// 当前运行环境
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

struct Segdesc gdt[NCPU + 5] = {
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
	// 每CPU TSS描述符 (从GD_TSS0开始) 在 trap_init_percpu() 中初始化
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
int envid2env(envid_t envid, struct Env **env_store, bool checkperm){
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
		env_p->env_parent_id = 0;
		env_p->env_type = ENV_TYPE_USER;
		env_p->env_status = 0;
		env_p->env_runs = 0;
		env_p->env_pgdir = NULL;

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
	lldt(0);
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
	//  - 不需要再调用 page_alloc
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
	e->env_tf.tf_esp = USTACKTOP;				// 这里设置用户栈顶
	e->env_tf.tf_cs = GD_UT | 3;
	// e->env_tf.tf_eip 稍后修改

	// 在用户模式下启用中断
	// LAB 4
	

	// 清除页面错误处理程序，直到用户安装一个
	e->env_pgfault_upcall = 0;

	// 也清除IPC接收标志
	e->env_ipc_recving = 0;

	// 提交分配结果
	env_free_list = e->env_link;
	*newenv_store = e;

	cprintf("[%08x] new env %08x\n", curenv ? curenv->env_id : 0, e->env_id);
	return 0;
}

/*
  为环境env分配len字节的物理内存，并将其映射到环境地址空间中的虚拟地址va；
  不要置零或以其他方式初始化映射页面；页面应该可被用户和内核写；如果分配失败则报错
 */
static void region_alloc(struct Env *e, void *va, size_t len){
	uint32_t cur_va = (uint32_t)ROUNDDOWN(va, PGSIZE);
	uint32_t end_va = (uint32_t)ROUNDUP(va + len, PGSIZE);
	uint32_t last_page = 0xfffff000;

	cprintf("ELF load: region_alloc env[%08x] at va = %08x with len = %d\n", e->env_id, va, len);

	struct PageInfo *pp;
	while(cur_va < end_va && cur_va <= last_page){
		pp = page_alloc(0);
		if(!pp){
			panic("page_alloc failed\n");
		}
		// 用 page_insert 将页面 pp 插入到 页目录中
		if(page_insert(e->env_pgdir, pp, (void *)cur_va, PTE_U | PTE_W) != 0){
			panic("page_insert failed to alloc at %p of len %x\n", va, len);
		}
		cur_va += PGSIZE;
	}
}

/*
  设置用户进程的初始二进制程序，堆栈和处理器标志；
  此函数仅在内核初始化期间，在运行第一个用户模式环境之前调用；

  此函数将所有可加载段从ELF二进制映像加载到环境的用户内存中，从ELF程序头中指示的适当虚拟地址开始执行；
  同时，它将 在程序头中标记为被映射但实际上不存在于ELF文件中的这些段清零，即程序的bss部分；
  所有这一切都非常类似于我们的引导加载程序bootloader，除了bootloader需要从磁盘读取代码；
  最后，此函数映射一个页面为程序的初始栈帧；
  如果函数遇到问题则报错 panics
 */
static void load_icode(struct Env *e, uint8_t *binary){
	// 将每个程序段装入虚拟内存中的 ELF段头中指定的地址处；
	// 你应该只加载 ph->p_type == ELF_PROG_LOAD 的段；
	// 每个段的虚拟地址可以在 ph->p_va 中找到，其在内存中的大小可以在 ph->p_memsz 中找到；
	// ELF文件 从'binary + ph->p_offset'开始的 ph->p_filesz 字节，应该被复制到虚拟地址 ph->p_va；
	// 剩余内存应清零（ELF头应有 ph->p_filesz <= ph->p_memsz），使用上个实验的函数 分配和映射页面；
	// 
	// 所有页保护位现在应该是用户读/写；
	// ELF段不一定页面对齐，但你可以假设这个函数没有两个段会触及同一虚拟页面；可使用 region_alloc；
	// 如果可以将数据直接移动到 内存中ELF二进制中的虚拟地址处，则加载段简单得多；
	// 	所以在这个函数期间哪个页目录应该有效 ?
	// 您还必须对程序的入口点执行某些操作，以确保环境开始在那里执行

	struct Elf *elfhdr = (struct Elf *)binary;
	if(elfhdr->e_magic != ELF_MAGIC)
		panic("read ELF file failed: magic %x number error\n", elfhdr->e_magic);

	struct Proghdr *ph = (struct Proghdr *)(binary + elfhdr->e_phoff);
	struct Proghdr *eph = ph + elfhdr->e_phnum;

	lcr3(PADDR(e->env_pgdir));					// 为 环境 e 映射

	for(; ph < eph; ph++){
		if(ph->p_type != ELF_PROG_LOAD)
			continue;
		if(ph->p_filesz > ph->p_memsz)
			panic("file size is great than memory size\n");

		region_alloc(e, (void *)ph->p_va, ph->p_memsz);
		memmove((void *)ph->p_va, binary+ph->p_offset, ph->p_filesz);
		memset((void *)(ph->p_va + ph->p_filesz), 0, ph->p_memsz - ph->p_filesz);
	}

	e->env_tf.tf_eip = elfhdr->e_entry;			// 设置指令执行地址

	// 现在为程序的初始栈帧映射一个页面，在虚拟地址 USTACKTOP - PGSIZE 处
	region_alloc(e, (void *)(USTACKTOP - PGSIZE), PGSIZE);

	lcr3(PADDR(kern_pgdir));
}

/*
  使用env_alloc分配新的env，使用load_icode将命名的elf二进制加载到其中，并设置其env_type；
  此函数仅在内核初始化期间，在运行第一个用户模式环境之前调用；新的 env 的 parent_id 设为 0；
  env_create 只是初始化一个 env ，并未运行；
  包括设置 env 结构的各个字段、内存空间、加载程序文件，env_status 设置为 ENV_RUNNABLE 等
 */
void env_create(uint8_t *binary, enum EnvType type){
	struct Env *env;
	int r = env_alloc(&env, 0);
	if(r < 0){
		panic("env_create: %e\n", r);
		return;
	}

	load_icode(env, binary);
	env->env_type = type;
}

/*
  释放一个Env和它使用的所有内存
 */
void env_free(struct Env *e){
	pte_t *pte;
	uint32_t pdeno, pteno;
	physaddr_t pa;

	// 如果释放当前环境，在释放页目录之前切换到kern_dir，以防页面重用
	if(e == curenv)
		lcr3(PADDR(kern_pgdir));

	cprintf("[%08x] free env %08x\n", curenv ? curenv->env_id : 0, e->env_id);

	// 清空地址空间的用户部分（UTOP以下）的所有映射页面
	static_assert(UTOP % PTSIZE == 0);
	for(pdeno = 0; pdeno < PDX(UTOP); pdeno++){
		// 只查看映射过的页表
		if(!(e->env_pgdir[pdeno] & PTE_P))
			continue;

		// 找到页表的 pa 和 va
		pa = PTE_ADDR(e->env_pgdir[pdeno]);
		pte = (pte_t *) KADDR(pa);

		for(pteno = 0; pteno < NPTENTRIES; pteno++){
			if(pte[pteno] & PTE_P)
				page_remove(e->env_pgdir, PGADDR(pdeno, pteno, 0));
		}

		// 释放页表自身
		e->env_pgdir[pdeno] = 0;
		page_decref(pa2page(pa));
	}
	// 释放页目录自身
	pa = PADDR(e->env_pgdir);
	e->env_pgdir = 0;
	page_decref(pa2page(pa));

	e->env_status = ENV_FREE;
	e->env_link = env_free_list;
	env_free_list = e;
}

/*
  释放用户环境，进入内核监控
  如果e是当前env，则运行一个新环境（并且不返回到调用者）
 */
void env_destroy(struct Env *e){
	// 如果e当前正在其他CPU上运行，我们将其状态更改为 ENV_DYING
	// 僵尸环境将在下次陷入内核时释放
	if(e->env_status == ENV_RUNNING && curenv != e){
		e->env_status = ENV_DYING;
		return;
	}

	env_free(e);

	if(curenv == e){
		curenv = NULL;
		sched_yield();
	}
	
	// cprintf("Destroyed the only environment - Enter the monitor!\n");
	// while(1)
	// 	monitor(NULL);
}

/*
  使用 iret 指令恢复Trapframe中的寄存器值；退出内核并开始执行 用户环境的代码；
  该函数不会返回
 */
void env_pop_tf(struct Trapframe *tf){
	// 记录我们正在运行的CPU用于用户空间调试
	curenv->env_cpunum = cpunum();

	asm volatile(
		"\tmovl %0,%%esp\n"
		"\tpopal\n"
		"\tpopl %%es\n"
		"\tpopl %%ds\n"
		"\taddl $0x8,%%esp\n"		/* skip tf_trapno and tf_errcode */
		"\tiret\n"				/* iret 指令 跳转到用户空间执行 */
		: : "g" (tf) : "memory");
	panic("iret failed");		/* mostly to placate the compiler */
}

/*
  上下文从当前环境 curent 切换到环境 e；如果这是第一次调用env_run，current是NULL；
  该函数不会返回
 */
void env_run(struct Env *e){
	// 如果这是一个上下文切换（一个新环境将要运行）
	// 	1. 将当前环境（如果有）设置回ENV_RUNNABLE，如果它是ENV_RUNNING（考虑其他可能的状态）
	// 	2. 设置 'curenv' 到新环境
	// 	3. 将其状态 status 设置为ENV_RUNNING
	// 	4. 更新它的'env_runs'计数器
	// 	5. 使用 lcr3() 切换到其地址空间
	// 使用 env_pop_tf() 恢复环境的寄存器，并进入环境中的用户模式
	// 
	// 此函数从 e->env_tf 加载新环境的状态；
	// 回到上面写的代码，并确保已将 e->env_tf 的相关部分设置为合理的值

	if(curenv == NULL || e->env_id != curenv->env_id){
		if(curenv != NULL && curenv->env_status == ENV_RUNNING)
			curenv->env_status = ENV_RUNNABLE;

		curenv = e;
		curenv->env_status = ENV_RUNNING;
		curenv->env_runs++;

		lcr3(PADDR(curenv->env_pgdir));
	}

	unlock_kernel();

	env_pop_tf(&e->env_tf);
}
