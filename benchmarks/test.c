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

void times4(void* arg) {
	int i = (*(int*) arg) * 4;
	rpthread_exit(&i);
}

void plus51(void* arg) {
	int i = 50 + *(int*) arg;
	rpthread_t t1;
	rpthread_create(&t1, NULL, times4, &i);
	int *ret;
	rpthread_join(t1, &ret);
	*ret += 1;
	rpthread_exit(ret);
}

int main(int argc, char **argv) {

	int* i = malloc(sizeof(int));
	*i = 1;
	rpthread_t t1;
	rpthread_create(&t1, NULL, plus51, i);
	rpthread_join(t1, &i);
	printf("Got back %d\n", *i);

	puts("exiting...");

	rpthread_exit(NULL);
}
