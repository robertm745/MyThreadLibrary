// File:	rpthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "rpthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

int tid;

tcb* runqueue;
tcb* finishedqueue;

ucontext_t* exitctx;

ucontext_t* schedctx;
struct sigaction* sa;
struct itimerval* timer;

tcb* currthread;

tcb* popThread(tcb** queue);
void pushThread(tcb** queue, tcb* thr);

static void schedule();
static void sched_rr();
static void sched_mlfq();

/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {
	// Your own implementation of RR
	// (feel free to modify arguments and return types)

	if (currthread->status == SCHEDULED)
		currthread->status = FINISHED;
	pushThread(&runqueue, currthread);
	currthread = popThread(&runqueue);
	if (currthread->status == READY) {
		currthread->status = SCHEDULED;
		// printf("Scheduling %u\n", currthread->id);
		setitimer(ITIMER_PROF, timer, NULL); 
		setcontext(&currthread->ctx);
	} else
		setcontext(schedctx);
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

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	puts("ERROR"); // for now
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
	if (*queue == NULL)
		return NULL;
	tcb* ptr = *queue;
	*queue = ptr->next;
	return ptr;
}

void exitfn() {
	printf("Entered exit fn, currthread->id is %u\n", currthread->id);
	// free() as needed
	// setcontext(schedctx);
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
		sa->sa_handler = (void(*)(int)) &rpthread_yield; 
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
		// currthread->status = SCHEDULED;
		currthread->next = NULL;

		// atexit((void*) exitfn);
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
	makecontext(&newTcb->ctx, (void(*)(void)) function, 1, arg); // used for setting context to the function-parameter directly
	pushThread(&runqueue, newTcb);

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
	puts("Exiting current thread...");

	// then free other contents of tcb...?
};

tcb* findtcb(rpthread_t thr) {
	tcb* ptr = runqueue;
	while (ptr) {
		if (ptr->id == thr) {
			return ptr;
		}
		ptr = ptr->next;
	}
	return NULL;
}

/* Wait for thread termination */
int rpthread_join(rpthread_t thread, void **value_ptr) {
	// wait for a specific thread to terminate
	// de-allocate any dynamic memory created by the joining thread

	tcb* target = findtcb(thread);
	if (target == NULL) {
		puts("join target NULL"); // shouldn't happen
		return 0;
	} 
	while (target->status != FINISHED) {
		// printf("In %u, %u not finished yet\n", currthread->id, thread);
		rpthread_yield();
	}
	if (value_ptr) {
		*value_ptr = target->retval;
	}
	printf("%u finished, returning from join...\n", target->id);
	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
		const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE
	// *mutex = (rpthread_mutex_t) malloc(sizeof(rpthread_mutex_t));
	mutex->init = 1;
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
		while (mutex->lock) {
			rpthread_yield();
		}
	}
	// mutex->owner = currthread->id;


	return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	__sync_lock_release(&mutex->lock);
	// mutex->owner = -1;
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init
	// free(mutex); ?
	return 0;
};


// Feel free to add any other functions you need

// YOUR CODE HERE
