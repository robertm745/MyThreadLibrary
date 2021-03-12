// File:	rpthread.c

// List all group member's name: Robert Mannifield, Allen Zhang
// username of iLab: rmm288
// iLab Server: ls.cs.rutgers.edu

#include "rpthread.h"

// thread and mutex IDs
int tid;
int mutid;

// RR queue
tcb* rrqueue;
// MLFQ
tcb* mlfq[4];

// scheduler's context
ucontext_t* schedctx;
// signal handler for timer
struct sigaction* sa;
struct itimerval* timer;
// currently running thread's tcb
tcb* currthread;
// dequeue a thread
tcb* popThread(tcb** queue);
// enqueue a thread
void pushThread(tcb** queue, tcb* thr);

// find the level of the highest priority thread that's ready to run
int findNextReadyLevel();
// set thread status(es) to READY if they're no longer blocked on a rpthread_join() call
void unblockThreadsFromJoin(tcb* thr);

/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {

	// If this thread is set as scheduled, it was not interrupted => it is finished
	if (currthread->status == SCHEDULED)
		currthread->status = FINISHED;
	// push current and get next thread
	pushThread(&rrqueue, currthread);
	currthread = popThread(&rrqueue);
	// set status, start the timer, and set context to next thread
	currthread->status = SCHEDULED;
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	int level;
	// If this thread is set as scheduled, it was not interrupted => it is finished
	if (currthread->status == SCHEDULED) {
		// printf("Thread %d finished\n", currthread->id);
		// find any threads that were waiting on this thread to finish
		unblockThreadsFromJoin(currthread);
		currthread->status = FINISHED;
	}
	// printf("Pushing thread %d with status %d to level %d\n", currthread->id, currthread->status, currthread->level);
	// push current, find highest level with a thread that's ready to run, then pop from that level
	pushThread(mlfq + currthread->level, currthread);
	level = findNextReadyLevel();
	currthread = popThread(mlfq + level);
	/*
	if (!currthread) {
		puts("MLFQ - NULL returned from popThread()");
		return;
	}
	*/
	// set status, start the timer, and set context to next thread
	currthread->status = SCHEDULED;
	// printf("Scheduling %d\n", currthread->id);
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library 
	// should be contexted switched from thread context to this 
	// schedule function

	// schedule policy
#ifndef MLFQ
	// Choose RR
	sched_rr();
#else 
	// Choose MLFQ
	sched_mlfq();
#endif
}

// finds the highest priority level that contains at least one thread that is ready to run
int findNextReadyLevel() {
	int level = 0;
	tcb* ptr;
	// start at level 0 i.e. highest priority
	while (level < 4) {
		ptr = mlfq[level];
		while (ptr) {
			// check if ptr thread is ready or if ptr thread is current thread
			if (ptr->status == READY || ptr->status == SCHEDULED) {
				// printf("Next ready is thread %d on level %d\n", ptr->id, level);
				return level;
			}
			ptr = ptr->next;
		}
		level++;
	}
	puts("Error: no threads ready to run in any level");
	return -1;
}

// Push a tcb to the back of queue passed to the function
void pushThread(tcb** queue, tcb* thr) {
	tcb* ptr = *queue;
	// if NULL, then set first tcb of queue to this thread
	if (!ptr) {
		*queue = thr;
	} else {
		// go the back of the queue, and set to NULL
		while (ptr->next) {
			ptr = ptr->next;
		}
		ptr->next = thr;
	}
	thr->next = NULL;
} 

// dequeue a tcb that is ready to run from the queue passed
tcb* popThread(tcb** queue) {
	tcb* ptr = *queue;
	// find the first tcb in the queue that is ready
	while (ptr->status != READY) {
		// advnace queue and push ptr to the back of the queue
		*queue = (*queue)->next;
		pushThread(queue, ptr);
		ptr = *queue;
	}
	*queue = (*queue)->next;
	return ptr;
}

// util function to free a context
void freectx(ucontext_t* c) {
        if (c != NULL) {
		free(c->uc_stack.ss_sp);
		free(c);
	}
}

// util function to clean up memory upon main thread's exit
void exitfn() {
	// free contexts, timer, queue(s)
        freectx(schedctx);
        free(sa);
        free(timer);
	tcb* ptr, *tmp;
#ifndef MLFQ
        ptr = rrqueue;
        while (ptr) {
            tmp = ptr->next;
            free(ptr->ctx.uc_stack.ss_sp);
	    free(ptr);
	    ptr = tmp;
	}
#else
	for (int i = 0; i < 4; i++) {
		ptr = mlfq[i];
		while (ptr) {
			tmp = ptr->next;
			free(ptr->ctx.uc_stack.ss_sp);
			free(ptr);
			ptr = tmp;
		}
	}
#endif
	puts("Exiting...");
}

// util function (only used for MLFQ) to serve as intermediary between timer interrupt and call to rpthread_yield(); used to demote priority
void timerfn() {
//	printf("Interrupted %d ", currthread->id);
	if (currthread->level < 3) {
		currthread->level++;
//		printf("demoted priority to %d", currthread->level);
	}
//	putchar('\n');
	rpthread_yield();
}

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	// create Thread Control Block
	// create and initialize the context of this thread
	// allocae space of stack for this thread to run
	// after everything is all set, push this thread int

	// check if scheduler context (schedctx) is initialized
	if (schedctx == NULL) {
		// initialize scheduler context
		schedctx = malloc(sizeof(ucontext_t));
		getcontext(schedctx);
		schedctx->uc_stack.ss_sp = malloc(SIGSTKSZ);
		schedctx->uc_stack.ss_size = SIGSTKSZ;
		schedctx->uc_stack.ss_flags = 0;
		schedctx->uc_link = NULL;
		makecontext(schedctx, (void (*)(void)) &schedule, 0);

		// set timer function 
		sa = malloc(sizeof(struct sigaction));
		#ifndef MLFQ
		sa->sa_handler = (void(*)(int)) &rpthread_yield; 
		#else
		sa->sa_handler = (void(*)(int)) &timerfn; 
		#endif
		sigaction (SIGPROF, sa, NULL);
		timer = malloc(sizeof(struct itimerval));
		timer->it_interval.tv_usec = 0;
		timer->it_interval.tv_sec = 0;
		timer->it_value.tv_usec = TIMESLICE;
		timer->it_value.tv_sec = 0;

		// create tcb for current context (context for function that called rpthread_create) and push to runqueue
		currthread = (tcb*) malloc(sizeof(tcb));
		getcontext(&currthread->ctx);
		currthread->id = tid++;
		currthread->next = NULL;
		currthread->level = 0;
		currthread->blockingMutex = -1;
		currthread->blockingThread = -1;

		atexit((void*) exitfn);

	}

	// create and set tcb for function that was passed to rpthread_create() and then push it to runqueue
	tcb* newTcb = (tcb*) malloc(sizeof(tcb));
	newTcb->id = tid;
	*thread = tid++;
	getcontext(&newTcb->ctx);
	newTcb->ctx.uc_stack.ss_sp = malloc(SIGSTKSZ);
	newTcb->ctx.uc_stack.ss_size = SIGSTKSZ;
	newTcb->ctx.uc_stack.ss_flags = 0;
	newTcb->ctx.uc_link = schedctx;
	newTcb->next = NULL;
	newTcb->status = READY;
	newTcb->level = 0;
	newTcb->blockingMutex = -1;
	newTcb->blockingThread = -1;
	makecontext(&newTcb->ctx, (void(*)(void)) function, 1, arg); // used for setting context to the function-parameter directly
	#ifndef MLFQ
	pushThread(&rrqueue, newTcb);
	#else 
	pushThread(mlfq, newTcb);
	#endif

	rpthread_yield();
	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {
	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// wwitch from thread context to scheduler context
	
	// set status as ready and switch back to scheduler
	currthread->status = READY;
	swapcontext(&currthread->ctx, schedctx);
	return 0;
};

/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// collect return value
	if (value_ptr) {
		currthread->retval = value_ptr;
	}
	// check if any threads were waiting on this thraed and set their status(es) to ready
	unblockThreadsFromJoin(currthread);
	currthread->status = FINISHED;
};

// util function to search a given queue for a matching thread
tcb* searchTcb(tcb** queue, rpthread_t thr) {
	tcb* ptr = *queue;
	while (ptr) {
		if (ptr->id == thr) {
			return ptr;
		}
		ptr = ptr->next;
	}
	return NULL;
}

// util function to find a thread in the appropriate queue based on RR or MLFQ
tcb* findtcb(rpthread_t thr) {
	#ifndef MLFQ
	return searchTcb(&rrqueue, thr);
	#else
	int i;
	tcb* ptr;
	for (i = 0; i < 4; i++) {
		if ((ptr = searchTcb(mlfq + i, thr)) != NULL)
			return ptr;
	}
	return NULL;
	#endif
}

/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	tcb* target;
	// Attempt to find matching thread; if not found, then return after notifying print statement
	if ((target = findtcb(thread)) == NULL) {
		puts("Error: target not found");
		return 1;
	}
	// wait while thread is not finished
	while (target->status != FINISHED) {
		// printf("%d still waiting on %d\n", currthread->id, target->id);
		// if RR, then simply yield - BLOCKED not needed as all thread's will get a chance to run
		#ifndef MLFQ
		rpthread_yield();
		#else
		// If MLFQ, then set to BLOCKED, set thread that this thread is blocked on, and go to scheduler
		// Note: BLOCKED status is needed as this thread may be blocked on a lower priority thread - lower priority thread will never run
		//  if this thread is not set to BLOCKED
		currthread->status = BLOCKED;
		currthread->blockingThread = target->id;
		swapcontext(&currthread->ctx, schedctx);
		#endif
	}
	// collect return value from target thread
	if (value_ptr) {
		*value_ptr = target->retval;
	}
	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	mutex->init = 1;
	mutex->id = mutid++;
	mutex->lock = 0;
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
	// use the built-in test-and-set atomic function to test the mutex
	// if the mutex is acquired successfully, enter the critical section
	// if acquiring mutex fails, push current thread into block list and //  
	// context switch to the scheduler thread

	// Attempt to acquire the mutex with atomic operation
	while (__sync_lock_test_and_set(&mutex->lock, 1)) {
		// /printf("Failed to acquire lock: %d blocked on mutex %d\n", currthread->id, mutex->id);
		// if lock was not acquired, set status and blocking mutex then go to scheduler
		while (mutex->lock) {
			// printf("%d blocked on mutex %d - mutex status %d\n", currthread->id, mutex->id, mutex->lock);
			currthread->blockingMutex = mutex->id;
			currthread->status = BLOCKED;
			swapcontext(&currthread->ctx, schedctx);
			// rpthread_yield();
		}
	}
	// printf("Thread %d acquired mutex %d\n", currthread->id, mutex->id);

	return 0;
};

// util function to traverse the queue (either RR or MLFQ) to find any threads that can be set to READY that are currently blocked on the given mutex
void unblockThreadsOnMutex(rpthread_mutex_t* mutex) {
	tcb* ptr;
	#ifndef MLFQ
	ptr = rrqueue;
	while (ptr) {
		// if BLOCKED on matching mutex, set status to READY
		if (ptr->status == BLOCKED && ptr->blockingMutex == mutex->id) {
			// printf("Set %d back to READY from mutex %d -- current thread %d\n", ptr->id, mutex->id, currthread->id);
			ptr->status = READY;
			// ptr->blockingMutex = -1;
		}
		ptr = ptr->next;
	}
	#else
	int i;
	for (i = 0; i < 4; i++) {
		ptr = mlfq[i];
		while (ptr) {
			// if BLOCKED on matching mutex, set status to READY
			if (ptr->status == BLOCKED && ptr->blockingMutex == mutex->id) {
				// printf("Set %d back to READY from mutex %d -- current thread %d\n", ptr->id, mutex->id, currthread->id);
				ptr->status = READY;
				// ptr->blockingMutex = -1;
			}
			ptr = ptr->next;
		}
	}
	#endif
}

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// release lock
	__sync_lock_release(&mutex->lock);
	// printf("Thread %d released mutex %d\n", currthread->id, mutex->id);
	// Unblock any thraeds that were waiting on this lock
	unblockThreadsOnMutex(mutex);
	return 0;
};

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	mutex->init = 0;
	return 0;
};

// util function to traverse the queue (either RR or MLFQ) to find any threads that can be set to READY that are currently blocked on the given thread
void unblockThreadsFromJoin(tcb* thr) {
	tcb* ptr;
	#ifndef MLFQ
	ptr = rrqueue;
	while (ptr) {
		// check if BLOCKED on matching thread ID, if so then set status to READY
		if (ptr->status == BLOCKED && ptr->blockingThread == thr->id) {
			// printf("Set %d back to READY from join -- current thread %d\n", ptr->id, currthread->id);
			// ptr->blockingThread = -1;
			ptr->status = READY;
		}
		ptr = ptr->next;
	}
	#else
	int i;
	for (i = 0; i < 4; i++) {
		ptr = mlfq[i];
		while (ptr) {
			// check if BLOCKED on matching thread ID, if so then set status to READY
			if (ptr->status == BLOCKED && ptr->blockingThread == thr->id) {
				// printf("Set %d back to READY from join -- current thread %d\n", ptr->id, currthread->id);
				// ptr->blockingThread = -1;
				ptr->status = READY;
			}
			ptr = ptr->next;
		}
	}
	#endif
}
