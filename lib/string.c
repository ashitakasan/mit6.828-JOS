// 基本字符串例程。没有进行硬件优化，但不简单

#include <inc/string.h>

// 对于memset的/ memmove与用汇编使得在实际硬件上有些区别，但在模拟器上区别很大
// 这种方式使管道的运行速度快 3倍
#define ASM 1

/*
 计算字符串长度
 */
int strlen(const char *s){
	int n;
	for(n = 0; *s != '\0'; s++)
		n++;
	return n;
}

/*
 计算字符串长度，最多到 size大小
 */
int strnlen(const char *s, size_t size){
	int n;
	for(n = 0; size > 0 && *s != '\0'; s++, size--)
		n++;
	return n;
}


/*
 字符串拷贝，返回拷贝后字符串首地址，地址重复会发生错误
 */
char *strcpy(char *dst, const char *src){
	char *ret;
	ret = dst;
	while((*dst++ = *src++) != '\0')
		;
	return ret;
}

/*
 字符串拼接，返回目的字符串首地址
 */
char *strcat(char *dst, const char *src){
	int len = strlen(dst);
	strcpy(dst + len, src);
	return dst;
}

/*
 字符串拷贝，最多拷贝 size字节，返回目的字符串首地址
 */
char *strncpy(char *dst, const char *src, size_t size){
	size_t i;
	char *ret;
	ret = dst;

	for(i = 0; i < size; i++){
		*dst++ = *src;
		if(*src != '\0')
			src++;
	}
	return ret;
}

/*
 字符串拷贝，返回拷贝后字符串长度
 */
size_t strlcpy(char *dst, const char *src, size_t size){
	char *dst_in;
	dst_in = dst;

	if(size > 0){
		while(--size > 0 && *src != '\0')
			*dst++ = *src++;
		*dst = '\0';
	}
	return dst - dst_in;
}


/*
 字符串比较，返回大于0，则第一个字符串大，0则相等
 */
int strcmp(const char *p, const char *q){
	while(*p && *p == *q)
		p++, q++;
	return (int)((unsigned char)*p - (unsigned char)*q);
}

/*
 字符串比较，最多比较 n个字节
 */
int strncmp(const char *p, const char *q, size_t n){
	while(n > 0 && *p && *p == *q)
		n--, p++, q++;
	if(n == 0)
		return 0;
	else
		return (int)((unsigned char)*p - (unsigned char)*q);
}


/*
 字符串字符查找，返回字符第一次出现的地址，没有找到则返回 0
 */
char *strchr(const char *s, char c){
	for(; *s; s++){
		if(*s == c)
			return (char *)s;
	}
	return 0;
}

/*
 字符串查找字符，返回字符第一次出现的地址，没有找到则返回 0(NULL)
 */
char *strfind(const char *s, char c){
	for(; *s; s++)
		if(*s == c)
			break;
	return (char *)s;
}


#if ASM

/*
 内存初始化为 c
 */
void *memset(void *v, int c, size_t n){
	char *p;

	if (n == 0)
		return v;
	if ((int)v%4 == 0 && n%4 == 0) {
		c &= 0xFF;
		c = (c<<24)|(c<<16)|(c<<8)|c;
		asm volatile("cld; rep stosl\n"
			:: "D" (v), "a" (c), "c" (n/4)
			: "cc", "memory");
	} else
		asm volatile("cld; rep stosb\n"
			:: "D" (v), "a" (c), "c" (n)
			: "cc", "memory");
	return v;
}

/*
 内存移动
 */
void *memmove(void *dst, const void *src, size_t n){
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("std; rep movsl\n"
				:: "D" (d-4), "S" (s-4), "c" (n/4) : "cc", "memory");
		else
			asm volatile("std; rep movsb\n"
				:: "D" (d-1), "S" (s-1), "c" (n) : "cc", "memory");
		// Some versions of GCC rely on DF being clear
		asm volatile("cld" ::: "cc");
	} else {
		if ((int)s%4 == 0 && (int)d%4 == 0 && n%4 == 0)
			asm volatile("cld; rep movsl\n"
				:: "D" (d), "S" (s), "c" (n/4) : "cc", "memory");
		else
			asm volatile("cld; rep movsb\n"
				:: "D" (d), "S" (s), "c" (n) : "cc", "memory");
	}
	return dst;
}

#else

/*
 内存 地址v开始格式化为 c，最多格式化 n个字节
 */
void *memset(void *v, int c, size_t n){
	char *p;
	int m;

	p = v;
	m = n;
	while(--m >= 0)
		*p++ = c;
	return v;
}

/*
 内存移动，从dst移动到src，最多移动 n个字节，不会发生内存重叠错误
 */
void *memmove(void *dst, const void *src, size_t n){
	const char *s;
	char *d;

	s = src;
	d = dst;
	if(s < d && s + n > d){
		s += n;
		d += n;
		while(n-- > 0)
			*--d = *--s;
	}
	else{
		while(n-- > 0)
			*d++ = *s++;
	}
	return dst;
}

#endif


/*
 内存拷贝，最多拷贝 n个字节
 */
void * memcpy(void *dst, const void *src, size_t n){
	return memmove(dst, src, n);
}

/*
 内存大小比较，最多比较 n个字节
 */
int memcmp(const void *v1, const void *v2, size_t n){
	const uint8_t *s1 = (const uint8_t *)v1;
	const uint8_t *s2 = (const uint8_t *)v2;

	while(n-- > 0){
		if(*s1 != *s2)
			return (int)*s1 - (int)*s2;
		s1++, s2++;
	}
	return 0;
}

/*
 内存查找，返回字符 c第一次出现的地址
 */
void *memfind(const void *s, int c, size_t n){
	const void *ends = (const char *)s + n;
	for(; s < ends; s++){
		if(*(const unsigned char *)s == (unsigned char)c)
			break;
	}
	return (void *)s;
}

/**
 * 字符串转数字，返回转换后的数字
 * @param  s      	原字符串
 * @param  endptr 	不能转换的字符串首地址
 * @param  base   	转换进制
 */
long strtol(const char *s, char **endptr, int base){
	int neg = 0;
	long val = 0;

	while(*s == ' ' || *s == '\t')		// 去掉空白字符
		s++;

	if(*s == '+')						// 处理符号
		s++;
	else if(*s == '-')
		s++, neg = 1;

	// 处理进制
	if((base == 0 || base == 16) && (s[0] == '0' && s[1] == 'x'))
		s += 2, base = 16;
	else if(base == 0 && s[0] == '0')
		s++, base = 8;
	else if(base == 0)
		base = 10;

	while(1){						// 开始转换数字
		int dig;
		char ch = *s;

		if(ch >= '0' && ch <= '9')
			dig = ch - '0';
		else if(ch >= 'a' && ch <= 'f')
			dig = ch - 'a' + 10;
		else if(ch >= 'A' && ch <= 'F')
			dig = ch - 'A' + 10;
		else
			break;

		if(dig >= base)
			break;

		s++, val = (val * base) + dig;
		// 这里不检测 int溢出
	}

	if(endptr)
		*endptr = (char *)s;
	return (neg ? -val : val);
}
