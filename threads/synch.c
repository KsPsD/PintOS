/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();   // disable interrupt
	// sema->value == 0이면 waiter리스트에 추가하고 기다린다
	while (sema->value == 0) {
		// 원래 코드
		list_push_back (&sema->waiters, &thread_current ()->elem);

		// /* ----- project1: priority scheduling-sync ----- */
		// list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);   // set back interrupt situation
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();	// 인터럽트 끄기

	if (!list_empty (&sema->waiters)) {

		/* ----- project1: priority scheduling-sync ----- */
		list_sort(&sema->waiters, cmp_priority, NULL);	// waiters 우선순위 변경 생겼을 수 있으므로 내림차순 정렬

		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
	}
	sema->value++;

	/* ----- project1: priority scheduling-sync ----- */
	test_max_priority();	// unblock된 스레드가 ready_list에 들어감 --> running 스레드와 우선순위 비교
	intr_set_level (old_level);		// 인터럽트 복구
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	/* ----- project1: priority scheduling(3) ----- */
	struct thread *cur = thread_current();
	if (lock->holder) {				// lock holder가 이미 있다면
		cur->wait_on_lock = lock;	// cur이 기다리는 lock 리스트에 추가
		// lock_holder의 donation리스트에 cur 정렬하여 추가
		// 굳이 정렬을 해야 하는지: 왜냐하면 계속 priority 갱신되기 때문에 항상 높은 것만 여기 들어올 것!
		// list_push_back()을 사용해야 하는 것이 낫다
		// list_insert_ordered(&lock->holder->donations, &cur->donation_elem,
		// 					thread_compare_donate_priority, NULL);
		list_push_front(&lock->holder->donations, &cur->donation_elem);
		donate_priority();
	}

	sema_down (&lock->semaphore);		// lock하는 것은 semaphore을 0으로 만드는 행위

	/* ----- project1: priority schduling(3) ----- */
	cur->wait_on_lock = NULL;			// lock을 획득했으므로 기다리는 lock <- NULL
	lock->holder = thread_current ();	// lock한 주체 저장
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	// lock을 해제하려는 주체(current running tread)가 lock을 한 스레드인지 확인
	ASSERT (lock_held_by_current_thread (lock));

	/* ----- project1: priority schedule(3) ----- */
	remove_with_lock(lock);	// lock을 가진 cur running의 donation list에서 priority 빌려준 스레드 제거
	refresh_priority();		// priority 재조정

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock)); // 현재 실행 스레드가 락을 건 스레드인지 확인

	sema_init (&waiter.semaphore, 0);	// waiter.semaphore 0으로 초기화  --> 밑에 코드 sema_down에서 막힘
										// 나중에 누군가 signal해주면서 sema_up을 해주면(--> ready상태로 만들고 running이 되면 그때 밑에 sema_down부터 실행)
	// cond는 semaphore의 리스트: cond->waiters
	// cond->waiters에 semaphore_elem.elem을 추가하는 과정

	// 원래 코드
	list_push_back (&cond->waiters, &waiter.elem);

	// /* ----- project1: priority scheduling (2) ------ */
	// list_insert_ordered(&cond->waiters, &waiter.elem, sema_compare_priority, 0);	// 

	lock_release (lock);			// 락을 푸는 이유: 이친구는 이제 block상태가 될 것임(sema_down) -> 모니터에서 나감(unlock)
	sema_down (&waiter.semaphore);	// blocking 상태로 들어가 다른 스레드가 sema_up을 해줘서 ready -> running되었을때 다시 여기부터 실행
	lock_acquire (lock);			// 다시 모니터 내부로 들어온 것이므로 lock 획득
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters)){
		/* ----- project1: priority scheduling (2) ----- */
		list_sort(&cond->waiters, sema_compare_priority, 0);  	// wait 도중 priority 바뀔 수 있으니 sort를 한다고 한다..?
		sema_up (&list_entry (list_pop_front (&cond->waiters),	// unblock -> ready상태로 만듦
					struct semaphore_elem, elem)->semaphore);	
	}
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

/* ----- project1: priority scheduling (2) ----- */
bool
sema_compare_priority (const struct list_elem *l, const struct list_elem *s, void *aux UNUSED){
	struct semaphore_elem *l_sema = list_entry(l, struct semaphore_elem, elem);
	struct semaphore_elem *s_sema = list_entry(s, struct semaphore_elem, elem);

	// **** 주어진 list_elem의 주소 l로 이것이 포함된 진정한 semaphore_elem의 주소를 구하는 매크로 함수
	// 구조체 구조 파악: 
    // struct semaphore_elem { struct list_elem elem{
    //  											struct list_elem *prev,
    //											    struct list_elem *next },
    //						  struct semaphore semaphore{
    //  											unsigned value,
    //											    struct list waiters{
    //														struct list_elem head,	
    //														struct list_elem tail }}
 
    // elem.next 멤버만큼 offset 왼쪽으로 이동하면 struct semaphore_elem의 주소 --> 이후 캐스팅
    //    (struct semaphore_elem *)       & elem->next
    //                        |<-offset()->|     
    // in memory...    		  V            V
    // sturct semaphore_elem: || elem.prev | elem.next || semaphore.value | semaphore.waiters...|| 
	//						  ^            ^                              |
	//	    l_sema, s_sema 여기		     여기 l.next, s.next              ^
	//										waiter_l_sema, waiter_s_sema 여기

	struct list *waiter_l_sema = &(l_sema->semaphore.waiters);
	struct list *waiter_s_sema = &(s_sema->semaphore.waiters);

	// l_sema.semaphore의 waiting_list의 첫번째 원소의 priority와
	// s_sema.semaphore의 waiting_list의 첫번째 원소의 priority 대소 비교
	// 즉, 세마포어가 가지는 waiting_list의 첫번째 스레드들의 priority를 비교하기 위함
	return list_entry (list_begin (waiter_l_sema), struct thread, elem)->priority     
			> list_entry (list_begin (waiter_s_sema), struct thread, elem)->priority;
}

