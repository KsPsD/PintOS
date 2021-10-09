#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);


void halt(void);
void exit(int status);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. 
 * Model Specific Register는 control register로서 대표적으로 test나 debugging에 쓰임.*/

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
/* system call handler가 control을 가질 때,
 * system call number 가 rax에 있고 %rdi, %rsi, %rdx, %r10, %r8 and %r9 순서로 arguments가 전달됨
 * caller's register은 struct intr_frame에 접근 가능. (intr_frame 은 kernel stack에 있음)
 */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	
	/*------- Project 2-2. syscalls ------*/
	char *fn_copy;
	int siz;

	switch (f->R.rax)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
	}
	/*------- Project 2-2. end ------*/

	printf ("system call!\n");
	thread_exit ();
}

/*------- Project 2-2. syscalls ------*/
// Terminates Pintos by calling power_off(). No return
void halt(void)
{
	power_off();
}

// Terminates the current user program, returning status to the kernel
// End current thread, record exit status
// conventionally, status = 0은 성공, 0아닌 경우 에러를 나타냄
void exit(int status)
{
	struct thread *cur = thread_current();
	cur->exit_status = status;

	printf("%s, exit(%d\n", thread_name(), status); // Process Termination Message
	thread_exit();
}

// parent : returns pid of child on success or -1 on fail
// child : Returns 0
// %RBX, %RSP, %RBP, %R12, %R15 들은 callee-saved registers이므로 clone할 필요 없다. (?)
tid_t fork(const char *thread_name, struct intr_frame *f)
{
	return preocess_fork(thread_name, f);
}

/*------- Project 2-2. end ------*/