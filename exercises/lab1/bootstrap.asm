=> 0x7c00:	cli    
   0x7c01:	cld    
   0x7c02:	xor    %ax,%ax
   0x7c04:	mov    %ax,%ds
   0x7c06:	mov    %ax,%es
   0x7c08:	mov    %ax,%ss
   0x7c0a:	in     $0x64,%al
   0x7c0c:	test   $0x2,%al
   0x7c0e:	jne    0x7c0a
   0x7c10:	mov    $0xd1,%al
   0x7c12:	out    %al,$0x64
   0x7c14:	in     $0x64,%al
   0x7c16:	test   $0x2,%al
   0x7c18:	jne    0x7c14
   0x7c1a:	mov    $0xdf,%al
   0x7c1c:	out    %al,$0x60
   0x7c1e:	lgdtw  0x7c64
   0x7c23:	mov    %cr0,%eax
   0x7c26:	or     $0x1,%eax
   0x7c2a:	mov    %eax,%cr0
   0x7c2d:	ljmp   $0x8,$0x7c32
   0x7c32:	mov    $0xd88e0010,%eax
   0x7c38:	mov    %ax,%es
   0x7c3a:	mov    %ax,%fs
   0x7c3c:	mov    %ax,%gs
   0x7c3e:	mov    %ax,%ss
   0x7c40:	mov    $0x7c00,%sp 		# 设置栈指针，开始调用 C函数
   0x7c43:	add    %al,(%bx,%si)
   0x7c45:	call   0x7d06 			# bootmain 入口
   0x7c48:	add    %al,(%bx,%si)
   0x7c4a:	jmp    0x7c4a
   0x7c4c:	add    %al,(%bx,%si) 		# gdt
   0x7c4e:	add    %al,(%bx,%si)
   0x7c50:	add    %al,(%bx,%si)
   0x7c52:	add    %al,(%bx,%si)
   0x7c54:	(bad)  
   0x7c55:	incw   (%bx,%si)
   0x7c57:	add    %al,(%bx,%si)
   0x7c59:	lcall  $0xffff,$0xcf
   0x7c5e:	add    %al,(%bx,%si)
   0x7c60:	add    %dl,0xcf(%bp,%si)
   0x7c64:	pop    %ss 				# gdtdesc
   0x7c65:	add    %cl,0x7c(%si)
   0x7c68:	add    %al,(%bx,%si) 		# void waitdisk(void)
   0x7c6a:	push   %bp
   0x7c6b:	mov    $0x1f7,%dx 		# static inline uint8_t inb(int port)
   0x7c6e:	add    %al,(%bx,%si)
   0x7c70:	mov    %sp,%bp
   0x7c72:	in     (%dx),%al
   0x7c73:	and    $0xffc0,%ax 		# while ((inb(0x1F7) & 0xC0) != 0x40)
   0x7c76:	cmp    $0x40,%al
   0x7c78:	jne    0x7c72
   0x7c7a:	pop    %bp
   0x7c7b:	ret 						# waitdisk end
   0x7c7c:	push   %bp 				# void readsect(void *dst, uint32_t offset);
   0x7c7d:	mov    %sp,%bp
   0x7c7f:	push   %di
   0x7c80:	push   %bx
   0x7c81:	mov    0xc(%di),%bx
   0x7c84:	call   0x7c68 			# waitdisk();
   0x7c87:	(bad)  
   0x7c88:	(bad)  
   0x7c89:	mov    $0x1f2,%dx 		# static inline void outb(int port, uint32_t data)
   0x7c8c:	add    %al,(%bx,%si)
   0x7c8e:	mov    $0x1,%al
   0x7c90:	out    %al,(%dx) 			# outb(0x1F2, 1);
   0x7c91:	mov    $0xf3,%dl
   0x7c93:	mov    %bl,%al
   0x7c95:	out    %al,(%dx) 			# outb(0x1F3, offset);
   0x7c96:	mov    %bx,%ax
   0x7c98:	mov    $0xf4,%dl
   0x7c9a:	shr    $0x8,%ax
   0x7c9d:	out    %al,(%dx) 			# outb(0x1F4, offset >> 8);
   0x7c9e:	mov    %bx,%ax
   0x7ca0:	mov    $0xf5,%dl
   0x7ca2:	shr    $0x10,%ax
   0x7ca5:	out    %al,(%dx) 			# outb(0x1F5, offset >> 16);
   0x7ca6:	mov    %bx,%ax
   0x7ca8:	mov    $0xf6,%dl
   0x7caa:	shr    $0x18,%ax
   0x7cad:	or     $0xffe0,%ax
   0x7cb0:	out    %al,(%dx) 			# outb(0x1F6, (offset >> 24) | 0xE0);
   0x7cb1:	mov    $0x20,%al
   0x7cb3:	mov    $0xf7,%dl
   0x7cb5:	out    %al,(%dx) 			# outb(0x1F7, 0x20);	// cmd 0x20 - read sectors
   0x7cb6:	call   0x7c68  			# call waitdisk();
   0x7cb9:	(bad)  
   0x7cba:	decw   0x87d(%bp,%di) 	# static inline void insl(int port, void* addr, int cnt)
   0x7cbe:	mov    $0x80,%cx
   0x7cc1:	add    %al,(%bx,%si)
   0x7cc3:	mov    $0x1f0,%dx
   0x7cc6:	add    %al,(%bx,%si)
   0x7cc8:	cld    
   0x7cc9:	repnz insw (%dx),%es:(%di) 	# insl(0x1F0, dst, SECTSIZE / 4); //read a sector
   0x7ccb:	pop    %bx
   0x7ccc:	pop    %di
   0x7ccd:	pop    %bp
   0x7cce:	ret    					# readsect end
   0x7ccf:	push   %bp 				# void readseg(uint32_t pa, uint32_t count, uint32_t offset)
   0x7cd0:	mov    %sp,%bp
   0x7cd2:	push   %di
   0x7cd3:	push   %si
   0x7cd4:	mov    0x10(%di),%di 		# offset = (offset / SECTSIZE) + 1;
   0x7cd7:	push   %bx
   0x7cd8:	mov    0xc(%di),%si
   0x7cdb:	mov    0x8(%di),%bx
   0x7cde:	shr    $0x9,%di
   0x7ce1:	add    %bx,%si
   0x7ce3:	inc    %di
   0x7ce4:	and    $0xfe00,%bx
   0x7ce8:	(bad)  
   0x7ce9:	(bad)  
   0x7cea:	cmp    %si,%bx 			# while (pa < end_pa)
   0x7cec:	jae    0x7d00
   0x7cee:	push   %di
   0x7cef:	push   %bx
   0x7cf0:	inc    %di
   0x7cf1:	add    $0x200,%bx
   0x7cf5:	add    %al,(%bx,%si)
   0x7cf7:	call   0x7c7a 			# call readsect;
   0x7cfa:	(bad)  
   0x7cfb:	lcall  *0x5a(%bx,%si)
   0x7cfe:	jmp    0x7cea
   0x7d00:	lea    -0xc(%di),%sp
   0x7d03:	pop    %bx
   0x7d04:	pop    %si
   0x7d05:	pop    %di
   0x7d06:	pop    %bp
   0x7d07:	ret    					# readseg end
   0x7d08:	push   %bp 				# void bootmain(void)
   0x7d09:	mov    %sp,%bp
   0x7d0b:	push   %si
   0x7d0c:	push   %bx 				# struct Proghdr *ph, *eph;
   0x7d0d:	push   $0x0
   0x7d0f:	push   $0x1000
   0x7d12:	add    %al,(%bx,%si)
   0x7d14:	push   $0x0
   0x7d17:	add    %ax,(%bx,%si)
   0x7d19:	call   0x7ccd 			# call readseg
   0x7d1c:	(bad)  
   0x7d1d:	incw   0xcc4(%bp,%di) 	# if (ELFHDR->e_magic != ELF_MAGIC)
   0x7d21:	cmpw   $0x0,(%di)
   0x7d25:	add    %ax,(%bx,%si)
   0x7d27:	jg     0x7d6e
   0x7d29:	dec    %sp
   0x7d2a:	inc    %si
   0x7d2b:	jne    0x7d64 			# goto bad;
   0x7d2d:	mov    0x1c,%ax 			# ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
   0x7d30:	add    %ax,(%bx,%si)
   0x7d32:	movzww (%di),%si 		# eph = ph + ELFHDR->e_phnum;
   0x7d35:	sub    $0x0,%al
   0x7d37:	add    %ax,(%bx,%si)
   0x7d39:	lea    0x0(%bx,%si),%bx	# ph = (struct Proghdr *) ((uint8_t *) ELFHDR + ELFHDR->e_phoff);
   0x7d3d:	add    %ax,(%bx,%si)
   0x7d3f:	shl    $0x5,%si 			# eph = ph + ELFHDR->e_phnum;
   0x7d42:	add    %bx,%si
   0x7d44:	cmp    %si,%bx
   0x7d46:	jae    0x7d5e
   0x7d48:	pushw  0x4(%bp,%di)
   0x7d4b:	pushw  0x14(%bp,%di)
   0x7d4e:	add    $0x20,%bx 			# for (; ph < eph; ph++)
   0x7d51:	pushw  -0x14(%bp,%di)
   0x7d54:	call   0x7ccd 			# call readseg
   0x7d57:	(bad)  
   0x7d58:	incw   0xcc4(%bp,%di)
   0x7d5c:	jmp    0x7d44
   0x7d5e:	call   *(%di) 			# ((void (*)(void)) (ELFHDR->e_entry))();
   0x7d60:	sbb    %al,(%bx,%si)
   0x7d62:	add    %ax,(%bx,%si)
   0x7d64:	mov    $0x8a00,%dx 		# static inline void outw(int port, uint16_t data)
   0x7d67:	add    %al,(%bx,%si)
   0x7d69:	mov    $0x8a00,%ax
   0x7d6c:	(bad)  
   0x7d6d:	jmp    *-0x11(%bp)
   0x7d70:	mov    $0x8e00,%ax
   0x7d73:	(bad)  
   0x7d74:	jmp    *-0x11(%bp)
   0x7d77:	jmp    0x7d77 			# bad while(1)

# kernel
   0x7d79:	add    %al,(%bx,%si)
   0x7d7b:	add    %al,(%bx,%si)
   0x7d7d:	add    %al,(%bx,%si)
   0x7d7f:	add    %al,(%bx,%si)
   0x7d81:	add    %al,(%bx,%si)
   0x7d83:	add    %al,(%bx,%si)
   0x7d85:	add    %al,(%bx,%si)
   0x7d87:	add    %al,(%bx,%si)
   0x7d89:	add    %al,(%bx,%si)
   0x7d8b:	add    %al,(%bx,%si)
   0x7d8d:	add    %al,(%bx,%si)
   0x7d8f:	add    %al,(%bx,%si)
   0x7d91:	add    %al,(%bx,%si)
   0x7d93:	add    %al,(%bx,%si)
   0x7d95:	add    %al,(%bx,%si)
   0x7d97:	add    %al,(%bx,%si)
   0x7d99:	add    %al,(%bx,%si)
   0x7d9b:	add    %al,(%bx,%si)
   0x7d9d:	add    %al,(%bx,%si)
   0x7d9f:	add    %al,(%bx,%si)
   0x7da1:	add    %al,(%bx,%si)
   0x7da3:	add    %al,(%bx,%si)
