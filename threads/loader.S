/* This file is derived from source code used in MIT's 6.828
   course.  The original copyright notice is reproduced in full
   below. */

/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

/* 여기서 로딩은 pintos의 첫 실행시 진행되는 부팅의 과정을 나타낸다. 부팅이란 컴퓨터가 구동하여 기초적인
 초기화 작업을 수행하고 운영 체제를 읽어오는 일련의 작업을 말한다. 로더(Loader)는 이러한 부팅 작업을
 진행하는 프로그램으로 장치(Disk)의 첫 번째 섹터에 저장되어 있다. */

#include "threads/loader.h"
	
#### Kernel loader.

#### This code should be stored in the first sector of the hard disk.
#### When the BIOS runs, it loads this code at physical address
#### 0x7c00-0x7e00 (512 bytes).  Then it jumps to the beginning of it,
#### in real mode.  This code switches into protected mode (32-bit
#### mode) so that all of memory can accessed, loads the kernel into
#### memory, and jumps to the first byte of the kernel, where start.S
#### is linked.
	
/* BIOS가 로더(Loader)를 메모리에 로드한다. BIOS는 컴퓨터가 구동된 후 저장 장치의 
   첫 번째 섹터(Sector)를 탐색하여 이 장치가 부팅 가능 한지 검사한다.
 * Sector : Disk에서 데이터를 저장하는 최소 단위로 한 섹터 512 bytes
 * 이 첫 번째 섹터를 MBR(Master Boot Record)라 하고 부팅 가능 장치임을 나타내기 위해
   섹터의 마지막 2 bytes가 0xAA55 (Magic Number)의 값을 가져야 한다.
 * 부팅 가능 장치임이 판명되면 BIOS는 첫 번째 섹터를 읽어서 메모리의 0x7c00위치에 올려놓고
   작업을 시작 */

/* Flags in control register 0. */
#define CR0_PE 0x00000001      /* Protection Enable. */
#define CR0_EM 0x00000004      /* (Floating-point) Emulation. */
#define CR0_PG 0x80000000      /* Paging. */
#define CR0_WP 0x00010000      /* Write-Protect enable in kernel mode. */


.globl start
start:
	
# Code runs in real mode, which is a 16-bit segment.
	.code16

# Disable interrupts, because we will not be prepared to handle them
# in protected mode until much later.
# String instructions go upward (e.g. for "rep stosl" below).

	cli
	cld

# Set up data segments.

	subw %ax, %ax
	movw %ax, %es
	movw %ax, %ds

# Set up stack segment.
# Stack grows downward starting from us.
# We don't ever use the stack, but we call into the BIOS,
# which might.

	movw %ax, %ss
	movw $0x7c00, %sp
	
#### Enable A20.  Address line 20 is tied to low when the machine
#### boots, which prevents addressing memory about 1 MB(2^20). This code
#### fixes it.
/* A20 line (CPU's address line numbered 20) 
 * x86 기반의 CPU의 numbered 20 adddress bus line에 있는 and gate를 말한다.
 * 20 bit 에서 32bit 아키텍처로 넘어가는 기점에서 원래의 20bit를 기반으로 구현된 프로그램을
 * 실행할 때, 아키텔처 차이로 인한 오버플로우를 막고 하위호환을 위해 20번째 address bus line에
 * and gate를 달아서 20bit 아키텍처에서와 같은 동작을 하도록 만든 gate이다. */
	
# Poll status register while busy.

1:	inb $0x64, %al
	testb $0x2, %al
	jnz 1b

# Send command for writing output port.

	movb $0xd1, %al
	outb %al, $0x64

# Poll status register while busy.

1:	inb $0x64, %al
	testb $0x2, %al
	jnz 1b

# Enable A20 line.

	movb $0xdf, %al
	outb %al, $0x60


#### Get memory size, via interrupt 15h function e820h. Now our pintos
#### runs on x86_64, "Get Extended Memory Size" operation is not sufficient
#### for our purpose.
	mov $0xe820, %eax  # command
	mov $(E820_MAP4), %edi  # dst
	xor %ebx, %ebx
	mov $0x534d4150, %edx # magic
	mov $24, %ecx
	int $0x15
	cmp %eax, %edx
	test %ebx, %ebx
	je panic
	mov $24, %ebp

parse_e820:
	mov %ecx, -4(%edi)
	add $24, %edi
	mov $0xe820, %eax
	mov $24, %ecx
	int $0x15
	jc e820_parse_done
	add $24, %ebp
	test %ebx, %ebx
	jne parse_e820

e820_parse_done:
	mov %ecx, -4(%edi)
	movl $0x40, MULTIBOOT_FLAG
	movl %ebp, MULTIBOOT_MMAP_LEN
	movl $(E820_MAP), MULTIBOOT_MMAP_ADDR # e820

#### Switch to protected mode.

# Note that interrupts are still off.

# Point the GDTR to our GDT.  Protected mode requires a GDT.
# We need a data32 prefix to ensure that all 32 bits of the GDT
# descriptor are loaded (default is to load only 24 bits).

	data32 lgdt gdtdesc

# Then we turn on the following bits in CR0:
#    PE (Protect Enable): this turns on protected mode.
#    PG (Paging): turns on paging.
#    WP (Write Protect): if unset, ring 0 code ignores
#       write-protect bits in page tables (!).
#    EM (Emulation): forces floating-point instructions to trap.
#       We don't support floating point. 
	
	movl %cr0, %eax
	orl $CR0_PE, %eax
	movl %eax, %cr0
	
# We're now in protected mode in a 16-bit segment.  The CPU still has
# the real-mode code segment cached in %cs's segment descriptor.  We
# need to reload %cs, and the easiest way is to use a far jump.
# Because we're not in a 32-bit segment the data32 prefix is needed to
# jump to a 32-bit offset.

	data32 ljmp $SEL_KCSEG, $protcseg

# We're now in protected mode in a 32-bit segment.

	.code32

# Reload all the other segment registers and the stack pointer to
# point into our new GDT.

protcseg:
	movw $SEL_KDSEG, %ax
	movw %ax, %ds		
	movw %ax, %es		
	movw %ax, %fs		
	movw %ax, %gs		
	movw %ax, %ss
	movl $LOADER_PHYS_BASE + 0x30000, %esp

#### Load kernel starting at physical address LOADER_PHYS_BASE by
#### frobbing the IDE controller directly.

	movl $1, %ebx
	movl $LOADER_PHYS_BASE, %edi

# Disable interrupt delivery by IDE controller, because we will be
# polling for data.
# (If we don't do this, Bochs 2.2.6 will never deliver any IDE
# interrupt to us later after we reset the interrupt controller during
# boot, even if we also reset the IDE controller.)

	movw $0x3f6, %dx
	movb $0x02, %al
	outb %al, %dx
	
read_sector:

# Poll status register while controller busy.

	movl $0x1f7, %edx
1:	inb %dx, %al
	testb $0x80, %al
	jnz 1b

# Read a single sector.

	movl $0x1f2, %edx
	movb $1, %al
	outb %al, %dx

# Sector number to write in low 28 bits.
# LBA mode, device 0 in top 4 bits.

	movl %ebx, %eax
	andl $0x0fffffff, %eax
	orl $0xe0000000, %eax

# Dump %eax to ports 0x1f3...0x1f6.

	movl $4, %ecx
1:	incw %dx
	outb %al, %dx
	shrl $8, %eax
	loop 1b

# READ command to command register.

	incw %dx
	movb $0x20, %al
	outb %al, %dx

# Poll status register while controller busy.

1:	inb %dx, %al
	testb $0x80, %al
	jnz 1b

# Poll status register until data ready.

1:	inb %dx, %al
	testb $0x08, %al
	jz 1b

# Transfer sector.

	movl $256, %ecx
	movl $0x1f0, %edx
	rep insw

# Next sector.

	incl %ebx
	cmpl $KERNEL_LOAD_PAGES*8 + 1, %ebx
	jnz read_sector

#### Jump to kernel entry point.
	movl $LOADER_PHYS_BASE, %eax
	call *%eax
	jmp panic

#### GDT

gdt:
	.quad 0x0000000000000000	# null seg
	.quad 0x00cf9a000000ffff	# code seg
	.quad 0x00cf92000000ffff  # data seg
	
gdtdesc:
	.word	0x17			# sizeof (gdt) - 1
	.long	gdt

#### Fatal error.
#### Print panic_message (with help from the BIOS) and spin.

panic:  .code16			# We only panic in real mode.
	movw $panic_message, %si
	movb $0xe, %ah
	subb %bh, %bh
1:	lodsb
	test %al, %al
2:	jz 2b			# Spin.
	int $0x10
	jmp 1b

panic_message:
	.ascii "Panic!"
	.byte 0

#### Command-line arguments and their count.
#### This is written by the `pintos' utility and read by the kernel.
#### The loader itself does not do anything with the command line.
	.org LOADER_ARG_CNT - LOADER_BASE
arg_cnt:
	.long 0
	.org LOADER_ARGS - LOADER_BASE
args:
	.fill 0x80, 1, 0

#### Boot-sector signature.
#### The BIOS checks that this is set properly.
	.org LOADER_SIG - LOADER_BASE
	.word 0xaa55 # 섹터의 마지막 2bytes가 0xAA55이어야 부팅 가능한 장치임을 나타냄.(Magic Number)
# 부팅 가능한 장치임이 판명되면 BIOS는 이 첫 번 째 섹터를 읽어서 메모리의 0x7c00 위치에 올려놓고 작업을 시작한다.