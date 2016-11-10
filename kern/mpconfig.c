#include <inc/types.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/x86.h>
#include <inc/mmu.h>
#include <inc/env.h>

#include <kern/cpu.h>
#include <kern/pmap.h>

struct CpuInfo cpus[NCPU];
struct CpuInfo *bootcpu;
int ismp;
int ncpu;

unsigned char percpu_kstacks[NCPU][KSTKSIZE] __attribute__((aligned(PGSIZE)));

// 浮动指针
struct mp {
	uint8_t signature[4];
	physaddr_t physaddr;			// MP配置表的物理地址
	uint8_t length;
	uint8_t specrev;
	uint8_t checksum;			// 所有字节必须加起来为0
	uint8_t type;				// MP系统配置类型
	uint8_t imcrp;
	uint8_t reserved[3];
}__attribute__((__packed__));

// 配置表头
struct mpconf {
	uint8_t signature[4];		// "PCMP"
	uint16_t length;				// 总表长度
	uint8_t version;				// [14]
	uint8_t checksum;			// 所有字节必须加起来为0
	uint8_t product[20];			// 产品 ID
	physaddr_t oemtable;			// OEM 表指针
	uint16_t oemlength;			// OEM 表长度
	uint16_t entry;				// 条目计数
	physaddr_t lapicaddr;		// 本地 APIC 地址
	uint16_t xlength;			// 扩展表长度
	uint8_t xchecksum;			// 扩展表校验和
	uint8_t reserved;
	uint8_t entries[0];			// 表条目
}__attribute__((__packed__));

// 处理器表条目
struct mpproc {
	uint8_t type;				// 条目类型
	uint8_t apicid;				// 本地 APIC ID
	uint8_t version;				// 本地 APIC 版本
	uint8_t flags;				// CPU 标志
	uint8_t signature[4];		// CPU签名
	uint32_t feature;			// 来自 CPUID 指令的特征标志
	uint8_t reserved[8];
}__attribute__((__packed__));

// mpproc 标志
#define MPPROC_BOOT	0x02			// 这个mpproc是引导处理器

// 表条目类型
#define MPPROC		0x00			// One per processor
#define MPBUS		0x01			// One per bus
#define MPIOAPIC		0x02			// One per I/O APIC
#define MPIOINTR		0x03			// One per bus interrupt source
#define MPLINTR		0x04			// One per system interrupt source


static uint8_t sum(void *addr, int len){
	int i, sum;
	sum = 0;
	for(i = 0; i < len; i++)
		sum += ((uint8_t *)addr)[i];
	return sum;
}

/*
  在物理地址 addr 处的 len 字节中查找 MP结构
 */
static struct mp *mpsearch1(physaddr_t a, int len){
	struct mp *mp = KADDR(a), *end = KADDR(a + len);
	for(; mp < end; mp++){
		if(memcpy(mp->signature, "_MP_", 4) == 0 && sum(mp, sizeof(*mp)) == 0)
			return mp;
	}
	return NULL;
}

/*
  搜索MP浮点指针结构，根据[MP 4]，它在以下三个位置之一:
  	1. 在 EBDA 的第一个 KB
  	2. 如果没有 EBDA，则在系统基本内存的最后一个KB
  	3. 在BIOS ROM中的0xE0000和0xFFFFF之间
 */
static struct mp *mpsearch(void){
	uint8_t *bda;
	uint32_t p;
	struct mp *mp;

	static_assert(sizeof(*mp) == 16);

	// BIOS数据区存在于16位段0x40中
	bda = (uint8_t *) KADDR(0x40 << 4);

	// [MP 4] END的16位段位于从BDA的字节0x0E开始的两个字节中
	if((p = *(uint16_t *)(bda + 0x0E))){
		p <<= 4;					// 从段翻译到物理地址
		if((mp = mpsearch1(p, 1024)))
			return mp;
	}
	else{
		// 基本存储器的大小（以KB为单位）位于从BDA的0x13开始的两个字节中
		p = *(uint16_t *)(bda + 0x13) * 1024;
		if((mp = mpsearch1(p - 1024, 1024)))
			return mp;
	}
	return mpsearch1(0xF0000, 0x10000);
}

/*
  搜索MP配置表；
  现在，不接受默认配置 (physaddr == 0)；检查正确的签名，校验和和版本
 */
static struct mpconf *mpconfig(struct mp **pmp){
	struct mpconf *conf;
	struct mp *mp;

	if((mp = mpsearch()) == 0)
		return NULL;
	if (mp->physaddr == 0 || mp->type != 0) {
		cprintf("SMP: Default configurations not implemented\n");
		return NULL;
	}
	conf = (struct mpconf *) KADDR(mp->physaddr);
	if (memcmp(conf, "PCMP", 4) != 0) {
		cprintf("SMP: Incorrect MP configuration table signature\n");
		return NULL;
	}
	if (sum(conf, conf->length) != 0) {
		cprintf("SMP: Bad MP configuration checksum\n");
		return NULL;
	}
	if (conf->version != 1 && conf->version != 4) {
		cprintf("SMP: Unsupported MP version %d\n", conf->version);
		return NULL;
	}
	if ((sum((uint8_t *)conf + conf->length, conf->xlength) + conf->xchecksum) & 0xff) {
		cprintf("SMP: Bad MP configuration extended checksum\n");
		return NULL;
	}

	*pmp = mp;
	return conf;
}

/*
  读取 CPU 配置，初始化 cpus，设置 bootcpu， 设置 lapicaddr
 */
void mp_init(void){
	struct mp *mp;
	struct mpconf *conf;
	struct mpproc *porc;
	uint8_t *p;
	unsigned int i;

	bootcpu = &cpus[0];
	if((conf = mpconfig(&mp)) == 0)
		return;
	ismp = 1;
	lapicaddr = conf->lapicaddr;

	for(p = conf->entries, i = 0; i < conf->entry; i++){
		switch(*p){
		case MPPROC:
			proc = (struct mpproc *)p;
			if (proc->flags & MPPROC_BOOT){
				bootcpu = &cpus[ncpu];
				cprintf("CPU %d is the bootstrap processor\n", ncpu);
			}
			if (ncpu < NCPU) {
				cpus[ncpu].cpu_id = ncpu;
				ncpu++;
			} else {
				cprintf("SMP: too many CPUs, CPU %d disabled\n", proc->apicid);
			}
			p += sizeof(struct mpproc);
			continue;
		case MPBUS:
		case MPIOAPIC:
		case MPIOINTR:
		case MPLINTR:
			p += 8;
			continue;
		default:
			cprintf("mpinit: unknown config type %x\n", *p);
			ismp = 0;
			i = conf->entry;
		}
	}

	bootcpu->cpu_status = CPU_STARTED;
	if(!ismp){
		ncpu = 1;
		lapicaddr = 0;
		cprintf("SMP: configuration not found, SMP disabled\n");
		return;
	}
	cprintf("SMP: CPU %d found %d CPU(s)\n", bootcpu->cpu_id,  ncpu);

	if(mp->imcrp){
		// 如果硬件实现 APIC 模式，则切换到从 LAPIC 获取中断
		cprintf("SMP: Setting IMCR to switch from PIC mode to symmetric I/O mode\n");
		outb(0x22, 0x70);				// 选择 IMCR
		outb(0x23, inb(0x23) | 1);		// 屏蔽外部中断
	}
}
