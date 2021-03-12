// File:	rpthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

//#define debug

#include "rpthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

int tid;
int mutid;

tcb* rrqueue;
tcb* mlfq[4];
tcb* blockedqueue;
tcb* finishedqueue;

volatile int* blockedNum;

ucontext_t* exitctx;

ucontext_t* schedctx;
struct sigaction* sa;
struct itimerval* timer;

tcb* currthread;

tcb* popThread(tcb** queue);
void pushThread(tcb** queue, tcb* thr);

static void sched_rr();
static void sched_mlfq();

int findNextReadyLevel();
void unblockThreadsFromJoin(tcb* thr);
void requeueReadyThreads();

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

/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {
	// Your own implementation of RR
	// (feel free to modify arguments and return types)
	
	if (currthread->status == SCHEDULED) {
		currthread->status = FINISHED;
 #ifdef debug
		printf("Pushing %d to finishedqueue\n", currthread->id);
 #endif
		pushThread(&finishedqueue, currthread);
		unblockThreadsFromJoin(currthread);
	} else if (currthread->status == BLOCKED) {
#ifdef debug
		printf("Pushing %d to blockedqueue\n", currthread->id);
#endif
		pushThread(&blockedqueue, currthread);
	} else {
#ifdef debug
		printf("Pushing %d to rrqueue\n", currthread->id);
#endif
		pushThread(&rrqueue, currthread);
	}
	if (*blockedNum) {
		requeueReadyThreads();
	}

	currthread = popThread(&rrqueue);
	currthread->status = SCHEDULED;
#ifdef debug
	printf("Scheduling %d\n", currthread->id);
#endif
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	int level;
	if (currthread->status == SCHEDULED) {
		// printf("Thread %d finished\n", currthread->id);
		unblockThreadsFromJoin(currthread);
		currthread->status = FINISHED;
	}
	// printf("Pushing thread %d with status %d to level %d\n", currthread->id, currthread->status, currthread->level);
	pushThread(mlfq + currthread->level, currthread);
	level = findNextReadyLevel();
	currthread = popThread(mlfq + level);
	if (!currthread) {
		puts("MLFQ - NULL returned from popThread()");
		return;
	}
	currthread->status = SCHEDULED;
	// printf("Scheduling %d\n", currthread->id);
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

int findNextReadyLevel() {
	int level = 0;
	tcb* ptr;
	while (level < 4) {
		ptr = mlfq[level];
		while (ptr) {
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

// enqueue a tcb
void pushThread(tcb** queue, tcb* thr) {
	tcb* ptr = *queue;
	if (!ptr) {
		*queue = thr;
	} else {
		while (ptr->next) {
			ptr = ptr->next;
		}
		ptr->next = thr;
	}
	thr->next = NULL;
} 

// dequeue a tcb
tcb* popThread(tcb** queue) {
	tcb* ptr = *queue;
	while (ptr->status != READY) {
		printf("Error: nonready in rrqueue - ptr->id: %d, currthread->id: %d\n", ptr->id, currthread->id);
		*queue = (*queue)->next;
		pushThread(queue, ptr);
		ptr = *queue;
	}
	*queue = (*queue)->next;
	return ptr;
}

void freectx(ucontext_t* c) {
        if (c != NULL) {
		free(c->uc_stack.ss_sp);
		free(c);
	}
}

void exitfn() {
	// free contexts
        freectx(schedctx);
        freectx(exitctx);
	// free timer
        free(sa);
        free(timer);
	// free queue(s)
        tcb* ptr = rrqueue, *tmp;
        while (ptr) {
            tmp = ptr->next;
            free(ptr->ctx.uc_stack.ss_sp);
	    free(ptr);
	    ptr = tmp;
	}
	puts("Exiting...");
}

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
		exitctx = (ucontext_t*) malloc(sizeof(ucontext_t));
		getcontext(exitctx);
		exitctx->uc_stack.ss_sp = malloc(SIGSTKSZ);
		exitctx->uc_stack.ss_size = SIGSTKSZ;
		exitctx->uc_stack.ss_flags = 0;
		exitctx->uc_link = schedctx;
		makecontext(exitctx, (void*) rpthread_exit, 1, NULL);

		blockedNum = (int*) malloc(sizeof(int));
		*blockedNum = 0;

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

	currthread->status = READY;
	swapcontext(&currthread->ctx, schedctx);
	return 0;
};

/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	if (value_ptr) {
		currthread->retval = value_ptr;
	}
};

// search util function
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

// find thread needed to be joined on
tcb* findtcb(rpthread_t thr) {
	tcb* ptr;
	#ifndef MLFQ
	if ((ptr = searchTcb(&rrqueue, thr)) != NULL) {
		return ptr;
	}
	#else
	int i;
	for (i = 0; i < 4; i++) {
		if ((ptr = searchTcb(mlfq + i, thr)) != NULL)
			return ptr;
	}
	#endif
	if ((ptr = searchTcb(&blockedqueue, thr)) != NULL) {
		return ptr;
	}
	return searchTcb(&finishedqueue, thr);
}

/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	tcb* target;
	if ((target = findtcb(thread)) == NULL) {
		puts("Error: target not found");
		return 1;
	}
	while (target->status != FINISHED) {
#ifdef debug
		printf("%d still waiting on %d\n", currthread->id, target->id);
#endif
		//currthread->status = BLOCKED;
		currthread->blockingThread = target->id;
		__sync_val_compare_and_swap(&currthread->status, currthread->status, BLOCKED);
		swapcontext(&currthread->ctx, schedctx);
	}
	if (value_ptr) {
		*value_ptr = target->retval;
	}
	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE
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

	// YOUR CODE HERE

	while (__sync_lock_test_and_set(&mutex->lock, 1)) {
#ifdef debug
		printf("Failed to acquire lock: %d blocked on mutex %d\n", currthread->id, mutex->id);
#endif
		while (mutex->lock) {
#ifdef debug
			printf("%d blocked on mutex %d - mutex status %d\n", currthread->id, mutex->id, mutex->lock);
#endif	
			currthread->blockingMutex = mutex->id;
			__sync_val_compare_and_swap(&currthread->status, currthread->status, BLOCKED);
			// currthread->status = BLOCKED;
			swapcontext(&currthread->ctx, schedctx);
		}
	}
#ifdef debug
	 printf("Thread %d acquired mutex %d\n", currthread->id, mutex->id);
#endif

	return 0;
};

void requeueReadyThreads() {
	tcb* ptr = blockedqueue, *prev = NULL;
	while (*blockedNum) {
#ifdef debug
		printf("Blocked num is %d\n", *blockedNum);
#endif
		if (ptr->status == READY) {
#ifdef debug
			printf("Found %d in blockedqueue as READY\n", ptr->id);
#endif
			ptr->blockingMutex = -1;
			ptr->blockingThread = -1;
			if (!prev) {
				blockedqueue = blockedqueue->next;
			} else {
				prev->next = ptr->next;
			}
			#ifndef MLFQ
			pushThread(&rrqueue, ptr);
			#else
			pushThread(mlfq + ptr->level, ptr);
			#endif
#ifdef debug
			printf("Pushed %d from blockedqueue back into rrqueue\n", ptr->id);
#endif
			if (__sync_sub_and_fetch(blockedNum, 1) == 0)
				break;
			ptr = prev;
		}
		prev = ptr;
		ptr = ptr->next;
	}
}

void unblockThreadsOnMutex(rpthread_mutex_t* mutex) {

	tcb* ptr = blockedqueue;
	while (ptr) {
		if (ptr->status == BLOCKED && ptr->blockingMutex == mutex->id) {
#ifdef debug
			printf("before unblock mutex: %d, %d is thread's id, status -- currthread->id %d\n", ptr->id, ptr->status, currthread->id);
#endif
			__sync_val_compare_and_swap(&ptr->status, BLOCKED, READY);
#ifdef debug
			printf("after unblock mutex: %d, %d is thread's id, status\n", ptr->id, ptr->status);
#endif
			__sync_fetch_and_add(blockedNum, 1);
		}
		ptr = ptr->next;
	}
}

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	__sync_lock_release(&mutex->lock);
#ifdef debug
	printf("Thread %d released mutex %d\n", currthread->id, mutex->id);
#endif
	unblockThreadsOnMutex(mutex);
	return 0;
};

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	mutex->init = 0;
	return 0;
};

void unblockThreadsFromJoin(tcb* thr) {
	tcb* ptr = blockedqueue;
	while (ptr) {
		if (ptr->status == BLOCKED && ptr->blockingThread == thr->id) {
#ifdef debug
			printf("before unblock join: %d, %d is thread's id, status\n", ptr->id, ptr->status);
#endif
			__sync_val_compare_and_swap(&ptr->status, BLOCKED, READY);
#ifdef debug
			printf("after unblock join: %d, %d is thread's id, status\n", ptr->id, ptr->status);
#endif
			__sync_fetch_and_add(blockedNum, 1);
		}
		ptr = ptr->next;
	}
}


// ignore for now...
	/*
	#ifndef MLFQ
	ptr = rrqueue;
	while (ptr) {
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
			if (ptr->status == BLOCKED && ptr->blockingThread == thr->id) {
				// printf("Set %d back to READY from join -- current thread %d\n", ptr->id, currthread->id);
				// ptr->blockingThread = -1;
				ptr->status = READY;
			}
			ptr = ptr->next;
		}
	}
	#endif
*/
