#ifndef JOS_INC_PARTITION_H
#define JOS_INC_PARTITION_H
#include <inc/types.h>

/*
  此文件包含x86分区表的定义，来自 Mike Mammarella
 */

// 分区扇区中的第一个分区描述符的偏移量
#define PTABLE_OFFSET		446

// 2字节分区表的魔术位置和值
#define PTABLE_MAGIC_OFFSET	510
#define PTABLE_MAGIC			"\x55\xAA"

// 分区类型常量
#define PTYPE_JOS_KERN		0x27			// JOS 内核
#define PTYPE_JOSFS			0x28			// JOS 文件系统

// 扩展分区标识符
#define PTYPE_DOS_EXTENDED	0x05
#define PTYPE_W95_EXTENDED	0x0F
#define PTYPE_LINUX_EXTENDED	0x85

struct Partitiondesc {
	uint8_t boot;
	uint8_t chs_begin[3];
	uint8_t type;
	uint8_t chs_end[3];
	uint32_t lba_start;
	uint32_t lba_length;
};

#endif
