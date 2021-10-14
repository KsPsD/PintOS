/*
#은 전처리기를 호출하는 특별한 문법이다. 그래서 #include도 전처리기이다.
#include 지시자는 헤더 파일(.h)와 소스 파일(.c)를 포함
헤더파일 지정시
  1) <> : 보통 C언어 표준 라이브러리의 헤더 파일을 포함할 때 사용. 또한 컴파일 옵션에서 지정한 헤더 파일 경로를 따른다.
  2) " " : 현재 소스 파일을 기준으로 헤더 파일을 포함. 찾지 못할 경우 컴파일 옵션에서 지정한 헤더 파일 경로를 따른다.

*/ 

#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "intrinsic.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* project1: threads - alarm clock */
static struct list sleep_list;
static int64_t next_tick_to_awake;
/* ------ project1 end ---------*/

/* Random value for basic thread
   Do not modify this value. */
#define THREAD_BASIC 0xd42df210

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Thread destruction requests */
static struct list destruction_req;

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static void do_schedule(int status);
static void schedule (void);
static tid_t allocate_tid (void);

/* Returns true if T appears to point to a valid thread. */
#define is_thread(t) ((t) != NULL && (t)->magic == THREAD_MAGIC)

/* Returns the running thread.
 * Read the CPU's stack pointer `rsp', and then round that
 * down to the start of a page.  Since `struct thread' is
 * always at the beginning of a page and the stack pointer is
 * somewhere in the middle, this locates the curent thread. */
#define running_thread() ((struct thread *) (pg_round_down (rrsp ())))


// Global descriptor table for the thread_start.
// Because the gdt will be setup after the thread_init, we should
// setup temporal gdt first.
static uint64_t gdt[3] = { 0, 0x00af9a000000ffff, 0x00cf92000000ffff };

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) {
	/* 스레드 시스템을 시작하기 위해 호출한다
	 * 핀토스의 첫번째 스레드를 만든다 */
	ASSERT (intr_get_level () == INTR_OFF);

	/* Reload the temporal gdt(global descriptor table?) for the kernel
	 * This gdt does not include the user context.
	 * The kernel will rebuild the gdt with user context, in gdt_init (). */
	struct desc_ptr gdt_ds = {
		.size = sizeof (gdt) - 1,
		.address = (uint64_t) gdt
	};
	lgdt (&gdt_ds);

	/* Init the global thread context */
	lock_init (&tid_lock);
	list_init (&ready_list);
	list_init (&destruction_req);

	/* ------- project1: alarm clock ----- */
	list_init(&sleep_list);

	/* Set up a thread structure for the running thread. */
	// init_thread로 thread구조체에 들어갈 정보 초기화
	initial_thread = running_thread ();
	init_thread (initial_thread, "main", PRI_DEFAULT);	// name: "main"과 priority: 31 부여
	initial_thread->status = THREAD_RUNNING;			// status: RUNNING 부여
	initial_thread->tid = allocate_tid ();				// TID 부여
}

/* Starts preemptive(선점하는 --> 도중에 중단 가능) thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) {
	/* 스케줄러를 시작하기 위해 호출한다
	 * ready 상태의 스레드가 없을 때, 스케줄되는 idle thread를 만든다

	/* Create the idle thread. */
	struct semaphore idle_started;

	sema_init (&idle_started, 0);
	thread_create ("idle", PRI_MIN, idle, &idle_started);  // "idle" 스레드 생성: init_thread -> 이름, priority 부여

	/* Start preemptive thread scheduling. */
	intr_enable ();		// 인터럽트 활성화

	/* Wait for the idle thread to initialize idle_thread. */
	sema_down (&idle_started);		// idle_started semaphore가 1이 될때까지 실행 안됨
									// 즉, thread_create(.., idle, &idle_started) 로 idle_thread 생성 후
									// idle_thread가 실행하면 idle함수에서 sema_up(idle)을 할때까지!
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) {
   /* timer tick의 timer interrupt가 호출한다
    * time slice가 만료되었을 때, 스케줄러를 가동한다
    * 스레드 통계 가지고 있다 */
	struct thread *t = thread_current ();

	/* Update statistics. */
	if (t == idle_thread)
		idle_ticks++;
#ifdef USERPROG
	else if (t->pml4 != NULL)
		user_ticks++;
#endif
	else
		kernel_ticks++;

	/* Enforce preemption. */
	// 4 tick 마다 intr_yield_on() --> inr_handler 마지막 thread_yield()
	// -> schedule() -> thread_ticks 초기화
	if (++thread_ticks >= TIME_SLICE)
		intr_yield_on_return ();
}

/* Prints thread statistics. */
void
thread_print_stats (void) {
	/* 스레드가 shutdown일때 호출된다
	 * 스레드 통계를 출력한다 */
	printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
			idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial <- 여기선 항상 kernel thread 만 만드는건가??
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority, thread_func *function, void *aux) {
	/* name으로 스레드를 만들고 시작
	 * 만들어진 스레드의 tid를 반환
	 * 이 스레드는 func 실행
	 * *aux는 func의 인자
	 * thread_create()는 스레드의 페이지를 할당하고, 스레드의 멤버들을 초기화하며, 스레드 스택을 할당한다
	 * 스레드는 blocked state 상태에서 초기화되며, 반환 직전에 unblock된다
	 * 이는 스레드가 스케줄할 수 있도록 하기 위함이다 */

	/* 동적할당 후(?) 스레드 name, priority, tid 부여 */
	struct thread *t;
	tid_t tid;

	ASSERT (function != NULL);

	/* Allocate thread. */
	t = palloc_get_page (PAL_ZERO); // 페이지 할당
	if (t == NULL)
		return TID_ERROR;

	/* Initialize thread. */

	init_thread (t, name, priority);

	// 2-4 File descriptor
	//t->fdTable = palloc_get_page(PAL_ZERO); // multi-oom : need more pages to accomodate 10 stacks of 126 opens
	t->fdTable = palloc_get_multiple(PAL_ZERO, FDT_PAGES);
	if (t->fdTable == NULL)
		return TID_ERROR;
	t->fdIdx = 2; // 0 : stdin, 1 : stdout
	// 2-extra
	t->fdTable[0] = 1; // dummy values to distinguish fd 0 and 1 from NULL
	t->fdTable[1] = 2;
	t->stdin_count = 1;
	t->stdout_count = 1;

	tid = t->tid = allocate_tid ();


	/*-----Project 2-3. Parent child-----*/
	struct thread *cur = thread_current();
	list_push_back(&cur->child_list, &t->child_elem); // [parent] add new child to child_list
	/*-----Project 2-3. end-----*/

	/* Call the kernel_thread if it scheduled.
	 * Note) rdi is 1st argument, and rsi is 2nd argument. */
	t->tf.rip = (uintptr_t) kernel_thread; // rip은 해당 스레드가 스케줄되면(cpu)에 올라가면 실행할 명령 주소
										   // 첫 명령으로 kernel_thread 실행
	t->tf.R.rdi = (uint64_t) function;	   // 그 인자로 function, aux 저장해 놓음
	t->tf.R.rsi = (uint64_t) aux;
	t->tf.ds = SEL_KDSEG;
	t->tf.es = SEL_KDSEG;
	t->tf.ss = SEL_KDSEG;
	t->tf.cs = SEL_KCSEG;
	t->tf.eflags = FLAG_IF;

	/* Add to run queue. */
	thread_unblock (t);

	/* ----- project1: priority scheduling ----- */
	// 새로만든 스레드가 ready_list에 들어갔으니, 현재 running 스레드와 ready_list의 첫번째와 우선순위 비교
	test_max_priority();

	return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) {
	/* running 상태의 스레드를 blocked 상태로 바꾼다.
	 * 이 스레드는 호출이 있지 않으면 다시 작동하지 않는다
	 * low-level syncrinization --> 다른 동기화 쓰는 것 추천(불냥이) */
	ASSERT (!intr_context ());
	ASSERT (intr_get_level () == INTR_OFF);
	thread_current ()->status = THREAD_BLOCKED;
	schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) {
	/* blocked 상태의 스레드를 ready 상태로 바꾼다 */
	enum intr_level old_level;

	ASSERT (is_thread (t));

	old_level = intr_disable ();
	ASSERT (t->status == THREAD_BLOCKED);

	// // 원래 코드: ready_list의 맨 뒤에 넣음
	// list_push_back (&ready_list, &t->elem);

	/* ----- project1: priority scheduling ----- */
	// ready_list에 추가할 때 정렬하면서 추가
	list_insert_ordered(&ready_list, &t->elem, cmp_priority, NULL);

	t->status = THREAD_READY;

	intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) {
	return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) {
	/* running 상태의 스레드를 반환한다 */
	struct thread *t = running_thread ();

	/* Make sure T is really a thread.
	   If either of these assertions fire, then your thread may
	   have overflowed its stack.  Each thread has less than 4 kB
	   of stack, so a few big automatic arrays or moderate
	   recursion can cause stack overflow. */
	ASSERT (is_thread (t));
	ASSERT (t->status == THREAD_RUNNING);

	return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) {
	/* running 상태 스레드의 tid를 반환한다
	 * thread_current()->tid와 같음 */
	return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) {
	/* 현재 스레드를 나가게함, 비가역적 */
	ASSERT (!intr_context ());

#ifdef USERPROG
	process_exit ();
#endif

	/* Just set our status to dying and schedule another process.
	   We will be destroyed during the call to schedule_tail(). */
	intr_disable ();
	do_schedule (THREAD_DYING);
	NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) {
	/* 현재 running중인 스레드를 비활성화하고, ready_list에 추가
	 * 스케줄링을 통해 새로운 스레드를 cpu에 할당 */
	struct thread *curr = thread_current ();
	enum intr_level old_level;

	ASSERT (!intr_context ());

	old_level = intr_disable ();	// 인터럽트 끄기
	if (curr != idle_thread)
		// // 원래 코드
		// list_push_back (&ready_list, &curr->elem);	// 현재 running thread를 ready_list에 추가

		/* ----- project1: priority scheduling ----- */
		list_insert_ordered(&ready_list, &curr->elem, cmp_priority, NULL);	// ready_list에 추가할 때, 정렬하면서 추가

	do_schedule (THREAD_READY);		// scheduling
	intr_set_level (old_level); 	// 인터럽트 복구
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) {
	/* 현재 running중인 스레드에 새로운 우선순위 부여 */

	// // 원래 코드
	// thread_current ()->priority = new_priority;

	/* ----- project1: priority scheduling ----- */
	if (!thread_mlfqs) { // priority가 조정되는 mlfqs가 발생하면 false
		int previous_priority = thread_current()->priority;
		thread_current()->init_priority = new_priority;

		refresh_priority ();

		// priority 감소한 경우 --> ready_list의 head와 한번 더 비교
		// test_max_priority();
		if (thread_current()->priority < previous_priority) {
			test_max_priority();
		}
	}
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) {
	/* 스레드의 우선순위 반환 */
	return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice UNUSED) {
	/* TODO: Your implementation goes here */
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) {
	/* TODO: Your implementation goes here */
	return 0;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) {
	struct semaphore *idle_started = idle_started_;

	idle_thread = thread_current ();
	sema_up (idle_started);

	for (;;) {
		/* Let someone else run. */
		intr_disable ();
		thread_block ();

		/* Re-enable interrupts and wait for the next one.

		   The `sti' instruction disables interrupts until the
		   completion of the next instruction, so these two
		   instructions are executed atomically.  This atomicity is
		   important; otherwise, an interrupt could be handled
		   between re-enabling interrupts and waiting for the next
		   one to occur, wasting as much as one clock tick worth of
		   time.

		   See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
		   7.11.1 "HLT Instruction". */
		asm volatile ("sti; hlt" : : : "memory");
	}
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) {
	ASSERT (function != NULL);
	/* scheduling 중에는 interrupt 안받음(disabled)
	 * scheduling 끝나고 해당 스레드(kernel thread) 시작하면
	 * 인터럽트 enable 후 함수 실행 -> 함수 종료시 kernel_thread 종료
	 * kernel_thread는 컴퓨터에서 수행하는 한 개의 task를 개념화한 것 */

	intr_enable ();       /* The scheduler runs with interrupts off. 
						   * 스케줄링 끝났으므로 다시 interrupt enable */
	function (aux);       /* Execute the thread function. */
	thread_exit ();       /* If function() returns, kill the thread. */
}


/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority) {
	ASSERT (t != NULL);
	ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
	ASSERT (name != NULL);

	memset (t, 0, sizeof *t);		// 메모리 0 초기화
	t->status = THREAD_BLOCKED;
	strlcpy (t->name, name, sizeof t->name);	// name 부여
	t->tf.rsp = (uint64_t) t + PGSIZE - sizeof (void *);
	t->priority = priority;			// priority 부여
	t->magic = THREAD_MAGIC;
	/*-----Project 1_priority scheduling_donation----*/
	// donation을 위한 변수들 초기화
	t->init_priority = priority;
	t->wait_on_lock = NULL;
	list_init(&t->donations);
	/*-----Project1 end----*/

	/*-----Project 2-3. syscall----*/
	list_init(&t->child_list);
	sema_init(&t->wait_sema, 0);
	sema_init(&t->fork_sema, 0);
	sema_init(&t->free_sema, 0);
	/*-----Project 2-3. end----*/

	t->running = NULL;

}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) {
	/* ready_list 비어있으면 idle_thread, 
	 * 아니면 ready_list의 맨 앞에서 꺼냄 */
	if (list_empty (&ready_list))
		return idle_thread;
	else
		// 리스트의 맨 앞 노드 포인터 thread 구조체를 가리키는 포인터로 변환(멤버 적절히..)
		return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Use iretq to launch the thread */
void
do_iret (struct intr_frame *tf) {
	__asm __volatile(
			"movq %0, %%rsp\n"
			"movq 0(%%rsp),%%r15\n"
			"movq 8(%%rsp),%%r14\n"
			"movq 16(%%rsp),%%r13\n"
			"movq 24(%%rsp),%%r12\n"
			"movq 32(%%rsp),%%r11\n"
			"movq 40(%%rsp),%%r10\n"
			"movq 48(%%rsp),%%r9\n"
			"movq 56(%%rsp),%%r8\n"
			"movq 64(%%rsp),%%rsi\n"
			"movq 72(%%rsp),%%rdi\n"
			"movq 80(%%rsp),%%rbp\n"
			"movq 88(%%rsp),%%rdx\n"
			"movq 96(%%rsp),%%rcx\n"
			"movq 104(%%rsp),%%rbx\n"
			"movq 112(%%rsp),%%rax\n"
			"addq $120,%%rsp\n"
			"movw 8(%%rsp),%%ds\n"
			"movw (%%rsp),%%es\n"
			"addq $32, %%rsp\n"
			"iretq"
			: : "g" ((uint64_t) tf) : "memory");
}

/* Switching the thread by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function. */
static void
thread_launch (struct thread *th) {
	/* 목적: 새로운 스레드가 기동함에 따라 context switcing을 수행한다 */
	uint64_t tf_cur = (uint64_t) &running_thread ()->tf;
	uint64_t tf = (uint64_t) &th->tf;
	ASSERT (intr_get_level () == INTR_OFF);		// interrupt off 상태 확보

	/* The main switching logic.
	 * We first restore the whole execution context into the intr_frame
	 * and then switching to the next thread by calling do_iret.
	 * Note that, we SHOULD NOT use any stack from here
	 * until switching is done. */
	__asm __volatile (
			/* Store registers that will be used. */
			"push %%rax\n"
			"push %%rbx\n"
			"push %%rcx\n"
			/* Fetch input once */
			"movq %0, %%rax\n"
			"movq %1, %%rcx\n"
			"movq %%r15, 0(%%rax)\n"
			"movq %%r14, 8(%%rax)\n"
			"movq %%r13, 16(%%rax)\n"
			"movq %%r12, 24(%%rax)\n"
			"movq %%r11, 32(%%rax)\n"
			"movq %%r10, 40(%%rax)\n"
			"movq %%r9, 48(%%rax)\n"
			"movq %%r8, 56(%%rax)\n"
			"movq %%rsi, 64(%%rax)\n"
			"movq %%rdi, 72(%%rax)\n"
			"movq %%rbp, 80(%%rax)\n"
			"movq %%rdx, 88(%%rax)\n"
			"pop %%rbx\n"              // Saved rcx
			"movq %%rbx, 96(%%rax)\n"
			"pop %%rbx\n"              // Saved rbx
			"movq %%rbx, 104(%%rax)\n"
			"pop %%rbx\n"              // Saved rax
			"movq %%rbx, 112(%%rax)\n"
			"addq $120, %%rax\n"
			"movw %%es, (%%rax)\n"
			"movw %%ds, 8(%%rax)\n"
			"addq $32, %%rax\n"
			"call __next\n"         // read the current rip.
			"__next:\n"
			"pop %%rbx\n"
			"addq $(out_iret -  __next), %%rbx\n"
			"movq %%rbx, 0(%%rax)\n" // rip
			"movw %%cs, 8(%%rax)\n"  // cs
			"pushfq\n"
			"popq %%rbx\n"
			"mov %%rbx, 16(%%rax)\n" // eflags
			"mov %%rsp, 24(%%rax)\n" // rsp
			"movw %%ss, 32(%%rax)\n"
			"mov %%rcx, %%rdi\n"
			"call do_iret\n"
			"out_iret:\n"
			: : "g"(tf_cur), "g" (tf) : "memory"
			);
}

/* Schedules a new process. At entry, interrupts must be off.
 * This function modify current thread's status to status and then
 * finds another thread to run and switches to it.
 * It's not safe to call printf() in the schedule(). */
static void
do_schedule(int status) {
	/* 현재 running중인 스레드를 주어진 status로 바꾸고, 새로운 스레드 실행
	 * 인터럽트가 없고, 현재 스레드 상태가 running일때 실행 */

	ASSERT (intr_get_level () == INTR_OFF);		// 스케줄링할때는 interrupt off 상태이어야 함
	ASSERT (thread_current()->status == THREAD_RUNNING);	// 현재 running thread의 상태가 THREAD_RUNNING 이어야 함

	// destruction_req: 소멸 확정 스레드 --> 순회하며 메모리 확보
	while (!list_empty (&destruction_req)) {
		struct thread *victim =	
			list_entry (list_pop_front (&destruction_req), struct thread, elem);
		palloc_free_page(victim); // Project 2-3. will be freed in 'process_wait'
	}
	thread_current ()->status = status;	// 현재 running 스레드의 상태 변경
	schedule (); 						// 실질적인 스케줄링
}

/*
 * scheduling을 한다 == schedule() 함수 실행
 * 핀토스에서 scheduling은 ready_list를 어떻게 관리하고, 
 * schedule()함수 안에서 호출되는 next_to_run 함수에 의해 다음 CPU를 점유할 kernel thread를 정할 때,
 * 어떤 kernel thread를 ready_list에서 꺼낼지에 대한 정책이다
 * 
 * 1) thread_yeild() 함수 호출시
 * 2) thread_block() 함수 호출시
 * 3) thread_exit() 함수 호출시
 * +) timer inturrupt 발생 -> intr_handler() -> timer_interrupt() -> thread_tick() -> thread_yield()
 * 4 tick마다 thread_yield() 실행
 */
static void
schedule (void) {
	/* 목적: running중인 스레드를 빼내고, next 스레드를 running으로 만든다
	 * curr: running 상태의 스레드
	 * next: ready_list가 있으면, ready_list의 첫번째 스레드를 가져온다(비어있으면 idle tread)
	 * next의 상태를 running으로 바꾸고, thread_tick을 0으로 바꾼다(새로운 스레드가 시작했으므로)
	 * 만약 curr이랑 next가 다른 스레드이고, curr이 dying상태이면서 initial_thread가 아니라면
	 * --> curr을 삭제 리스트의 마지막에 삽입한다
	 * next를 thread_launch()한다: context switching 수행 */
	struct thread *curr = running_thread ();		// 현재 running thread
	struct thread *next = next_thread_to_run ();	// next running thread: reay_list의 맨 앞 or idle_thread

	ASSERT (intr_get_level () == INTR_OFF);			// interrupt off 확인
	ASSERT (curr->status != THREAD_RUNNING);		// curr상태가 runnging에서 바뀌었는지 확인
	ASSERT (is_thread (next));

	/* Mark us as running. */
	next->status = THREAD_RUNNING;

	/* Start new time slice. */
	thread_ticks = 0;

#ifdef USERPROG
	/* Activate the new address space. */
	process_activate (next);
#endif

	if (curr != next) {
		/* If the thread we switched from is dying, destroy its struct
		   thread. This must happen late so that thread_exit() doesn't
		   pull out the rug under itself.
		   We just queuing the page free reqeust here because the page is
		   currently used bye the stack.
		   The real destruction logic will be called at the beginning of the
		   schedule(). */
		if (curr && curr->status == THREAD_DYING && curr != initial_thread) {
			ASSERT (curr != next);
			list_push_back (&destruction_req, &curr->elem);
		}

		/* Before switching the thread, we first save the information
		 * of current running. */
		thread_launch (next);
	}
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) {
	static tid_t next_tid = 1;
	tid_t tid;

	lock_acquire (&tid_lock);
	tid = next_tid++;
	lock_release (&tid_lock);

	return tid;
}


/* ----- project1: alarm clock ----- */
void
thread_sleep (int64_t ticks) { // from timer_sleep (start + ticks)
	struct thread *cur;
	cur = thread_current();

	// idle -> stop: idle_thread cannot sleep
	if (cur == idle_thread) {
		ASSERT(0);
	}
	else {
		enum intr_level old_level;
		old_level = intr_disable(); // interrupt off
		
		update_next_tick_to_awake(cur->wakeup_tick = ticks); // 일어날 시간 저장
		list_push_back(&sleep_list, &cur->elem); 			  // sleep_list에 추가
		thread_block();										  // block 상태로 변경
		
		intr_set_level(old_level);  // interrupt on
	}
}

/* ----- project1: alarm clock ----- */
void
thread_awake (int64_t wakeup_tick){ // from timer_interrupt. 즉 매 초마다 awake 할 게 있는지 확인후 있으면 이 함수 실행
	next_tick_to_awake = INT64_MAX;

	// take a sleeping thread
	struct list_elem *sleeping;
	sleeping = list_begin(&sleep_list);

	// sleep_list 순회 -> 깨워야 할 스레드를 sleep_list에서 제거하고 unblock
	while (sleeping != list_end(&sleep_list)){
		struct thread *th = list_entry(sleeping, struct thread, elem);

		// 일어날 시간이 된 스레드
		if (wakeup_tick >= th->wakeup_tick) {	// 스레드가 일어날 시간이 되었는지 확인
			sleeping = list_remove(&th->elem); 	// sleep_list에서 제거 후 next를 가리킴
			thread_unblock(th);					// unblock thread -> ready_list에 넣는다
		}
		// 일어날 시간이 안된 스레드
		else {
			sleeping = list_next(sleeping); 	// move to next sleeping thread
			// 순회하면서 가장 작은 wakeup_tick으로 갱신한다
			// 바로 다음에 깨울 스레드 시간
			update_next_tick_to_awake(th->wakeup_tick);
		}
	}
}

/* ----- project1: alarm clock ----- */
void
update_next_tick_to_awake(int64_t ticks){
	// find smallest tick
	next_tick_to_awake = (next_tick_to_awake > ticks) ? ticks : next_tick_to_awake; 
}

int64_t
get_next_tick_to_awake(void){
	return next_tick_to_awake;
}
/* ----- project1: alarm clock end -----*/

/* ----- project1: priority scheduling ----- */

/* 현재 running중인 스레드와 ready_list의 처음 스레드의
 * 우선순위 대소비교 후 yield() 수행 */
void
test_max_priority(void) {
	struct thread *cur = thread_current();
	struct thread *first_thread;

	if (list_empty(&ready_list)) return;

	first_thread = list_entry(list_front(&ready_list), struct thread, elem);

	if (cur->priority < first_thread->priority){		// 우선순위 대소비교
		thread_yield();
	}
}

bool
cmp_priority (const struct list_elem *a, const struct list_elem *b, void *aux UNUSED){
	// struct list_elem 포인터 --> struct thread 포인터 변환
	struct thread *thread_a = list_entry(a, struct thread, elem);
	struct thread *thread_b = list_entry(b, struct thread, elem);

	// a와 b의 priority 비교: a가 클 때 true return
	if (thread_a != NULL && thread_b != NULL) {
		if (thread_a->priority > thread_b->priority)
			return true;
		else
			return false;
	}

	return false;

}

/*-----Project1_priority scheduling_donation----*/
bool
thread_compare_donate_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED)
{
	return list_entry (l, struct thread, donation_elem)->priority
		 > list_entry (s, struct thread, donation_elem)->priority;
}

void
donate_priority (void)
{
	int depth;
	struct thread *cur = thread_current ();

	for (depth = 0; depth < 8; depth++){
		if (!cur->wait_on_lock) break;
		struct thread *holder = cur->wait_on_lock->holder; // 여기서 holder는 처음에 lock을 갖지만 우선순위가 낮을 수 있는 스레드
		holder->priority = cur->priority; // 우선순위가 낮을 수 있는 스레드에 현재 스레드의 priority 기부
		cur = holder; // 한칸 깊숙히 들어감. lock을 가졌던 thread(lock->holder)를 cur (thread)로 변경
	}
}

void
remove_with_lock (struct lock *lock)
{
	struct list_elem *e;
	struct thread *cur = thread_current ();

	for (e = list_begin (&cur->donations); e != list_end (&cur->donations); e = list_next (e)){
		struct thread *t = list_entry (e, struct thread, donation_elem);
		if (t->wait_on_lock == lock) // 현재 스레드가 들고있는 lock을 필요로 하는 스레드 찾기
			list_remove (&t->donation_elem);
	}
}

void
refresh_priority (void)
{
	struct thread *cur = thread_current ();

	cur->priority = cur->init_priority;

	if(!list_empty (&cur->donations)) {
		list_sort (&cur->donations, thread_compare_donate_priority, 0);

		struct thread *front = list_entry (list_front (&cur->donations), struct thread, donation_elem);
		if (front->priority > cur->priority)
			cur->priority = front->priority;
	}
}

/*-----Project1 end----*/