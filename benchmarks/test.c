#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include "../rpthread.h"

/* A scratch program template on which to call and
 * test rpthread library functions as you implement
 * them.
 *
 * You can modify and use this program as much as possible.
 * This will not be graded.
 */

void tempfn2() {
	puts("start fn 2\n");
	int i, j = 0;
	for (i = 0; i < 10000000; i++) {
		j++; // timer should interrupt at some point here
	}
	puts("end fn 2\n");
	rpthread_exit(NULL);
}

void tempfn() {
	rpthread_t t2;
	int i, j = 0;
	puts("start fn 1\n");
	for (i = 0; i < 10000000; i++) {
		j++; // timer should interrupt at some point here
	}
	puts("end fn 1\n");
	rpthread_create(&t2, NULL, tempfn2, NULL);
	rpthread_join(t2, NULL);
	rpthread_yield();
	rpthread_exit(NULL);
}

int main(int argc, char **argv) {

	/* Implement HERE */
	rpthread_t t, t2;
	rpthread_create(&t, NULL, tempfn, NULL);

	puts("another string!\n");

	rpthread_create(&t2, NULL, tempfn2, NULL);
	rpthread_join(t, NULL);
	rpthread_join(t2, NULL);
	rpthread_yield();
	/*
	sleep(3);
	int i, j = 0;
	for (i = 0; i < 100000000; i++) {
		j++; // timer should interrupt at some point here
	}
	*/
	puts("exiting...");

	return 0;
}
