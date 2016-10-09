#include <inc/stab.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>

#include <kern/kdebug.h>

extern const struct Stab __STAB_BEGIN__[];	// stab表开头
extern const struct Stab __STAB_END__[];		// stab表结束
extern const char __STABSTR_BEGIN__[];		// string表开头
extern const char __STABSTR_END__[];			// string表结束


/*
 stab_binsearch(stabs, region_left, region_right, type, addr)
 有些stab类型由指令地址以升序排列，如 N_FUN stabs（stab的 n_type == N_FUN）
 表示函数，而 N_SO stabs表示源文件。
 
 给定一个指令的地址，该函数查找包含指令地址的 stab表；
 搜索区间：[*region_left, *region_right]，
 因此，要搜索整个组N个stab表的，你可以这样做：
	left = 0;
	right = N - 1;
	stab_binsearch(stabs, &left, &right, type, addr);

 搜索修改* region_left和* region_right中的地址addr；
 *region_left指向包含地址addr匹配的stab，而* region_right指向下一个stab；
 如果* region_left>* region_right，那么地址不包含在任何匹配的stab。

 例如，给定这些 N_SO stabs：
	Index  Type   Address
	0      SO     f0100000
	13     SO     f0100040
	117    SO     f0100176
	118    SO     f0100178
	555    SO     f0100652
	556    SO     f0100654
	657    SO     f0100849
 代码：
 	left = 0, right = 657; 
 	stab_binsearch(stabs, &left, &right, N_SO, 0xf0100184);
 	会设置 left = 118, right = 554。
 */
static void stab_binsearch(const struct Stab *stabs, int *region_left,
				int *region_right, int type, uintptr_t addr){
	int l = *region_left, r = *region_right, any_matches = 0;

	// 二分查找
	while(l <= r){
		int true_m = (l + r) / 2, m = true_m;

		// 查找最早出现的类型为type的stab
		while(m >= l && stabs[m].n_type != type)
			m--;
		if(m < l){
			l = true_m + 1;
			continue;
		}

		any_matches = 1;
		if(stabs[m].n_value < addr){
			*region_left = m;
			l = true_m + 1;
		}
		else if(stabs[m].n_value > addr){
			*region_right = m - 1;
			r = m - 1;
		}
		else{
			*region_left = m;	// 完全匹配addr，仍然继续查找
			l = m;
			addr++;
		}
	}

	if(!any_matches)
		*region_right = *region_left - 1;
	else{
		// 发现包含'addr'最右边的区域
		for(l = *region_right; l > *region_left && stabs[l].n_type != type; l--)
			;
		*region_left = l;
	}
}

/*
 debuginfo_eip(addr, info);
 用指定EIP指令地址addr处的信息，填充 info 结构体；
 如果找到eip信息返回0；否则返回负值，同时在info中也会储存一些信息。
 */
int debuginfo_eip(uintptr_t addr, struct Eipdebuginfo *info){
	const struct Stab *stabs, *stab_end;
	const char *stabstr, *stabstr_end;

	int lfile, rfile, lfun, rfun, lline, rline;

	// 初始化 info
	info->eip_file = "<unknown>";
	info->eip_line = 0;
	info->eip_fn_name = "<unknown>";
	info->eip_fn_namelen = 9;
	info->eip_fn_addr = addr;
	info->eip_fn_narg = 0;

	if(addr >= ULIM){
		stabs = __STAB_BEGIN__;
		stab_end = __STAB_END__;
		stabstr = __STABSTR_BEGIN__;
		stabstr_end = __STABSTR_END__;
	}
	else{
		// 不能搜索用户级地址
		panic("User address");
	}

	// 字符串表的有效性检查
	if(stabstr_end <= stabstr || stabstr_end[-1] != 0)
		return -1;

	// 现在我们开始查找包含eip指令的函数所在的stab表；
	// 首先，查找包含eip指令的基本的源文件，然后在源文件中查找函数；
	// 最后，找到eip指令所在的文件行。
	
	// 查找源文件全部的类型的 N_SO 的 stabs表
	lfile = 0;
	rfile = (stab_end - stabs) - 1;
	stab_binsearch(stabs, &lfile, &rfile, N_SO, addr);
	if(lfile == 0)
		return -1;

	// 搜索源文件的stabs中函数定义
	lfun = lfile;
	rfun = rfile;
	stab_binsearch(stabs, &lfun, &rfun, N_FUN, addr);

	if(lfun <= rfun){
		// stabs[lfun] 指向字符串表内的函数名
		if(stabs[lfun].n_strx < stabstr_end - stabstr)
			info->eip_fn_name = stabstr + stabs[lfun].n_strx;
		info->eip_fn_addr = stabs[lfun].n_value;
		addr -= info->eip_fn_addr;

		// 查找该函数内的行号
		lline = lfun;
		rline = rfun;
	}
	else {
		// 没有找到函数stab，可能在汇编文件中，此时查找整个文件的文件行
		info->eip_fn_addr = addr;
		lline = lfile;
		rline = rfile;
	}
	// 忽略冒号后的东西
	info->eip_fn_namelen = strfind(info->eip_fn_name, ':') - info->eip_fn_name;

	// 在 [lline, rline]中查找文件行stab，
	// 如果找到了，就设置 info->eip_line 为文件行，否则返回 -1
	// 注意：文件行有特殊的 stabs类型，在 <inc/stab.h> 中定义
	
	stab_binsearch(stabs, &lline, &rline, N_SLINE, addr);
	if(lline <= rline)
		info->eip_line = stabs[lline].n_desc;
	else
		return -1;

	// 从相关文件名的文件行stab向后搜索。
	// 我们不能仅使用 lfile stab，因为内联函数从不同文件插入代码。
	// 这些包含的源文件使用 N_SOL stab类型
	while(lline >= lfile && stabs[lline].n_type != N_SOL
		&& (stabs[lline].n_type != N_SO || !stabs[lline].n_value))
		lline--;
	if(lline >= lfile && stabs[lline].n_strx < stabstr_end - stabstr)
		info->eip_file = stabstr + stabs[lline].n_strx;

	// 设置 eip_fn_narg为函数的参数数目，如果没有函数则设置为0
	if(lfun < rfun){
		for(lline = lfun + 1; lline < rfun && stabs[lline].n_type == N_PSYM; lline++)
			info->eip_fn_narg++;
	}
	return 0;
}
