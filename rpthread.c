// File:	rpthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "rpthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

int tid;
int mutid;

tcb* rrqueue;
tcb* mlfq[4];
tcb* finishedqueue;

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

	if (currthread->status == SCHEDULED)
		currthread->status = FINISHED;
	pushThread(&rrqueue, currthread);
	currthread = popThread(&rrqueue);
	currthread->status = SCHEDULED;
//	printf("Scheduling %d\n", currthread->id);
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	int level;
	if (currthread->status == SCHEDULED)
		currthread->status = FINISHED;
	pushThread(mlfq + currthread->level, currthread);
	level = findNextReadyLevel();
	currthread = popThread(mlfq + level);
	if (!currthread) {
		puts("MLFQ - NULL returned from popThread()");
		return;
	}
	currthread->status = SCHEDULED;
	setitimer(ITIMER_PROF, timer, NULL); 
	setcontext(&currthread->ctx);
}

int findNextReadyLevel() {
	int level = 0;
	tcb* ptr;
	while (level < 4) {
		ptr = mlfq[level];
		while (ptr) {
			if (ptr->status == READY) 
				return level;
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
		thr->next = NULL;
	}
} 

// dequeue a tcb
tcb* popThread(tcb** queue) {
	tcb* ptr = *queue;
	while (ptr->status != READY) {
		*queue = (*queue)->next;
		pushThread(queue, ptr);
		ptr = *queue;
	}
	*queue = (*queue)->next;
	return ptr;
	// #endif
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
}

void timerfn() {
	if (currthread->level < 3) 
		currthread->level++;
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
		sa->sa_handler = (void(*)(int)) &timerfn; 
		#else
		sa->sa_handler = (void(*)(int)) &rpthread_yield; 
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
		currthread->blockedOn = -1;

		atexit((void*) exitfn);
		exitctx = (ucontext_t*) malloc(sizeof(ucontext_t));
		getcontext(exitctx);
		exitctx->uc_stack.ss_sp = malloc(SIGSTKSZ);
		exitctx->uc_stack.ss_size = SIGSTKSZ;
		exitctx->uc_stack.ss_flags = 0;
		exitctx->uc_link = schedctx;
		makecontext(exitctx, (void*) rpthread_exit, 1, NULL);

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
	newTcb->blockedOn = -1;
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
	currthread->status = FINISHED;
};

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
	if ((target = findtcb(thread)) == NULL) {
		puts("Error: target not found");
		return 1;
	}
	while (target->status != FINISHED) {
//		printf("%d still waiting on %d\n", currthread->id, target->id);
		rpthread_yield();
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
//		printf("Failed to acquire lock: %d blocked on mutex %d\n", currthread->id, mutex->id);
		while (mutex->lock) {
//			printf("%d blocked on mutex %d\n", currthread->id, mutex->id);
			currthread->status = BLOCKED;
			currthread->blockedOn = mutex->id;
			swapcontext(&currthread->ctx, schedctx);
			// rpthread_yield();
		}
	}

	return 0;
};

void unblockThreads(rpthread_mutex_t* mutex) {
	tcb* ptr;
	#ifndef MLFQ
	ptr = rrqueue;
	while (ptr) {
		if (ptr->blockedOn == mutex->id && ptr != currthread) {
//			printf("Set %d back to READY from mutex %d -- current thread %d\n", ptr->id, mutex->id, currthread->id);
			ptr->blockedOn = -1;
			ptr->status = READY;
		}
		ptr = ptr->next;
	}
	#else
	int i;
	for (i = 0; i < 4; i++) {
		ptr = mlfq[i];
		while (ptr) {
			if (ptr->blockedOn == mutex->id && ptr != currthread) {
//				printf("Set %d back to READY from mutex %d -- current thread %d\n", ptr->id, mutex->id, currthread->id);
				ptr->blockedOn = -1;
				ptr->status = READY;
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

	// YOUR CODE HERE
	__sync_lock_release(&mutex->lock);
	unblockThreads(mutex);
	return 0;
};

/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	mutex->init = 0;
	return 0;
};


// Feel free to add any other functions you need

// YOUR CODE HERE
