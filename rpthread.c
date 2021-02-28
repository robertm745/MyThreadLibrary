// File:	rpthread.c

// List all group member's name:
// username of iLab:
// iLab Server:

#include "rpthread.h"

// INITAILIZE ALL YOUR VARIABLES HERE
// YOUR CODE HERE

int callnum;
tcb* runqueue;
tcb* finishedqueue;
ucontext_t* schedctx;
struct sigaction* sa;
struct itimerval* timer;

tcb* currthread;

tcb* popThread(tcb** queue);
void pushThread(tcb** queue, tcb* thr);

static void schedule();

// starts a thread and collects return value, if any
static void* threadStart(void*(*function)(void*), void* arg) {
	currthread->retval = function(arg);
	puts("fn done");
	currthread->status = FINISHED;
	swapcontext(&currthread->ctx, schedctx);
	return 0;
}

/* Round Robin (RR) scheduling algorithm */
static void sched_rr() {
	// Your own implementation of RR
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE

	// runqueue is never null in current approach
	while (1) {
		if (currthread->status == FINISHED) {
			pushThread(&finishedqueue, currthread);
		} else {
			pushThread(&runqueue, currthread);
		}
		currthread = popThread(&runqueue);
		if (currthread == NULL) { // shouldn't happen (?)
			puts("currthread NULL");
			break;
		}
		if (currthread->status == READY) {
			currthread->status = SCHEDULED;
			setitimer(ITIMER_PROF, timer, NULL); // if I comment this out, it then works as expected
			swapcontext(schedctx, &currthread->ctx);
			if (currthread->status == SCHEDULED) {
				puts("thread set sched -> ready");
				currthread->status = READY;
			} else {
				puts("thread finished");
			}
		} 
	}

}

void timerfn(int val) {
	// puts("timer went off");
	// printf("context id is %u\n", currthread->id);
	// setcontext(schedctx);
	// *(unsigned long int*)(&val + 47) = &rpthread_yield + 1;
	swapcontext(&currthread->ctx, schedctx);
}

/* scheduler */
static void schedule() {
	// Every time when timer interrup happens, your thread library 
	// should be contexted switched from thread context to this 
	// schedule function

	// Invoke different actual scheduling algorithms
	// according to policy (RR or MLFQ)

	// if (sched == RR)
	sched_rr();
	// else if (sched == MLFQ)
	//	sched_mlfq();

	// YOUR CODE HERE

	// schedule policy
#ifndef MLFQ
	// Choose RR
	// CODE 1
#else 
	// Choose MLFQ
	// CODE 2
#endif

}

/* Preemptive MLFQ scheduling algorithm */
static void sched_mlfq() {
	// Your own implementation of MLFQ
	// (feel free to modify arguments and return types)

	// YOUR CODE HERE
}

// enqueue a tcb
void pushThread(tcb** queue, tcb* thr) {
	tcb* ptr = *queue;
	char* s = (queue == &runqueue) ? "runqueue" : "finishedqueue";
	if (!ptr) {
		*queue = thr;
		printf("Pushed first tcb to %s\n", s);
		// puts("pushed first"); // debug
	} else {
		printf("Pushed another tcb to %s\n", s);
		// puts("pushed another"); // debug
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

/* create a new thread */
int rpthread_create(rpthread_t * thread, pthread_attr_t * attr, void *(*function)(void*), void * arg) {
	// create Thread Control Block
	// create and initialize the context of this thread
	// allocate space of stack for this thread to run
	// after everything is all set, push this thread int
	// YOUR CODE HERE


	// check if scheduler context (schedctx) is initialized
	if (schedctx == NULL) {
		puts("schedctx init"); //debug 
		schedctx = malloc(sizeof(ucontext_t));
		getcontext(schedctx);
		schedctx->uc_stack.ss_sp = malloc(SIGSTKSZ);
		schedctx->uc_stack.ss_size = SIGSTKSZ;
		schedctx->uc_stack.ss_flags = 0;
		schedctx->uc_link = NULL;
		makecontext(schedctx, (void (*)(void)) &schedule, 0);

		// set timer function 
		sa = malloc(sizeof(struct sigaction));
		sa->sa_handler = (void(*)(int)) &timerfn;
		sigaction (SIGPROF, sa, NULL);
		timer = malloc(sizeof(struct itimerval));
		timer->it_interval.tv_usec = 0;
		timer->it_interval.tv_sec = 0;
		timer->it_value.tv_usec = TIMESLICE;
		timer->it_value.tv_sec = 0;

		// create tcb for current context (context for function that called rpthread_create) and push to runqueue
		currthread = (tcb*) malloc(sizeof(tcb));
		getcontext(&currthread->ctx);
		currthread->id = 0;
		currthread->status = READY;
		currthread->next = NULL;

	}

	// create and set tcb for function that was passed to rpthread_create() and then push it to runqueue
	printf("rpthread call num %d\n", ++callnum); // debug
	tcb* newTcb = (tcb*) malloc(sizeof(tcb));
	newTcb->id = *(rpthread_t*) thread;
	getcontext(&newTcb->ctx);
	newTcb->ctx.uc_stack.ss_sp = malloc(SIGSTKSZ);
	newTcb->ctx.uc_stack.ss_size = SIGSTKSZ;
	newTcb->ctx.uc_stack.ss_flags = 0;
	newTcb->ctx.uc_link = schedctx;
	newTcb->next = NULL;
	newTcb->status = READY;
	makecontext(&newTcb->ctx, (void (*)(void)) &threadStart, 2, function, arg);

	// makecontext(&newTcb->ctx, (void(*)(void)) function, 1, arg); // used for setting context to the function-parameter directly

	puts("pushed new thread");
	pushThread(&runqueue, newTcb);

	// swap contexts to the scheduler
	swapcontext(&currthread->ctx, schedctx);
	return 0;
};

/* give CPU possession to other user-level threads voluntarily */
int rpthread_yield() {

	// change thread state from Running to Ready
	// save context of this thread to its thread control block
	// wwitch from thread context to scheduler context

	// YOUR CODE HERE 

	// puts("timer called"); // debug
	swapcontext(&currthread->ctx, schedctx);
	return 0;
};

/* terminate a thread */
void rpthread_exit(void *value_ptr) {
	// Deallocated any dynamic memory created when starting this thread

	// YOUR CODE HERE

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

	// YOUR CODE HERE
	tcb* target = findtcb(thread);
	if (target == NULL) {
		puts("join target NULL");
		return 0;
	} else {
		puts("join target found");
	}
	while (target->status != FINISHED) {
		rpthread_yield();
	}
	return 0;
};

/* initialize the mutex lock */
int rpthread_mutex_init(rpthread_mutex_t *mutex, 
		const pthread_mutexattr_t *mutexattr) {
	//initialize data structures for this mutex

	// YOUR CODE HERE
	return 0;
};

/* aquire the mutex lock */
int rpthread_mutex_lock(rpthread_mutex_t *mutex) {
	// use the built-in test-and-set atomic function to test the mutex
	// if the mutex is acquired successfully, enter the critical section
	// if acquiring mutex fails, push current thread into block list and //  
	// context switch to the scheduler thread

	// YOUR CODE HERE
	return 0;
};

/* release the mutex lock */
int rpthread_mutex_unlock(rpthread_mutex_t *mutex) {
	// Release mutex and make it available again. 
	// Put threads in block list to run queue 
	// so that they could compete for mutex later.

	// YOUR CODE HERE
	return 0;
};


/* destroy the mutex */
int rpthread_mutex_destroy(rpthread_mutex_t *mutex) {
	// Deallocate dynamic memory created in rpthread_mutex_init

	return 0;
};


// Feel free to add any other functions you need

// YOUR CODE HERE
/*
   int main(int argc, char* argv[]) {
   rpthread_t t1;
   rpthread_create(&t1, NULL, (void*) testFunc, NULL);
   return 0;
   }
 */
