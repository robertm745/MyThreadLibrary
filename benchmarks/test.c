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
	int i = (*(int*) arg), j;
	pthread_yield();
	i *= 4;
	pthread_yield();
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	rpthread_exit(&i);
}

void plus51(void* arg) {
	int i = 50 + *(int*) arg, j;
	rpthread_t t1;
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	rpthread_create(&t1, NULL, times4, &i);
	int *ret;
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	rpthread_join(t1, &ret);
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	*ret += 1;
	rpthread_exit(ret);
}

int main(int argc, char **argv) {

	int* i = malloc(sizeof(int)), j;
	*i = 1;
	rpthread_t t1;
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	rpthread_create(&t1, NULL, plus51, i);
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	for (j = 0; j < 100000000; j++) {
		j++;
		j--;
	}
	rpthread_join(t1, &i);
	printf("Got back %d\n", *i);

	puts("exiting...");

	return 0;
}
