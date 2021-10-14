#include "userprog/tss.h"
#include <debug.h>
#include <stddef.h>
#include "userprog/gdt.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/vaddr.h"
#include "intrinsic.h"

/* The Task-State Segment (TSS).
 *
 *  Instances of the TSS, an x86-64 specific structure, are used to
 *  define "tasks", a form of support for multitasking built right
 *  into the processor.  However, for various reasons including
 *  portability, speed, and flexibility, most x86-64 OSes almost
 *  completely ignore the TSS.  We are no exception.
 *  여러가지 이유로 보통 제대로 구현안됨
 *
 *  Unfortunately, there is one thing that can only be done using
 *  a TSS: stack switching for interrupts that occur in user mode.
 *  When an interrupt occurs in user mode (ring 3), the processor
 *  consults the rsp0 members of the current TSS to determine the
 *  stack to use for handling the interrupt.  Thus, we must create
 *  a TSS and initialize at least these fields, and this is
 *  precisely what this file does.
 *	//그래서 여기서도 약식으로 구현 인터럽트 될때 스택 스위칭 정도만 함
 *  When an interrupt is handled by an interrupt or trap gate
 *  (which applies to all interrupts we handle), an x86-64 processor
 *  works like this:
 *		
 *    - If the code interrupted by the interrupt is in the same
 *      ring as the interrupt handler, then no stack switch takes
 *      place.  This is the case for interrupts that happen when
 *      we're running in the kernel.  The contents of the TSS are
 *      irrelevant for this case.
 *		만약 인터럽트된 코드가 같은 링일때 스택이 변하지 않음 커널일때 일어나는 거임/

 *    - If the interrupted code is in a different ring from the
 *      handler, then the processor switches to the stack
 *      specified in the TSS for the new ring.  This is the case
 *      for interrupts that happen when we're in user space.  It's
 *      important that we switch to a stack that's not already in
 *      use, to avoid corruption.  Because we're running in user
 *      space, we know that the current process's kernel stack is
 *      not in use, so we can always use that.  Thus, when the
 *      scheduler switches threads, it also changes the TSS's
 *      stack pointer to point to the new thread's kernel stack.
 *      (The call is in schedule in thread.c.) */
		//만약 중단된 코드가 다른 링에 존재하면 스위치한다 .이 케이스에서는 우리가 유저 스페이스에 있음
		// 아직 사용중이지 않은 스택으로 이동해야됨 corruption 피하기 위해서. 우리가 유저 스택이면
		// 현재 커널 스택이 사용중이지 않으니, 우린 계속 사용가능함. 따라서 컨텍스트 스위칭 일어나면 tss스택 포인터를 커널 스택으로 옮김

/* Kernel TSS. */
struct task_state *tss;

/* Initializes the kernel TSS. */
void tss_init(void)
{
	/* Our TSS is never used in a call gate or task gate, so only a
	 * few fields of it are ever referenced, and those are the only
	 * ones we initialize. */
	tss = palloc_get_page(PAL_ASSERT | PAL_ZERO);
	tss_update(thread_current());
}

/* Returns the kernel TSS. */
struct task_state *
tss_get(void)
{
	ASSERT(tss != NULL);
	return tss;
}

/* Sets the ring 0 stack pointer in the TSS to point to the end
 * of the thread stack. */
void tss_update(struct thread *next)
{
	ASSERT(tss != NULL);
	tss->rsp0 = (uint64_t)next + PGSIZE;
}
