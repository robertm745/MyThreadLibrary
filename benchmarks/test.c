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

rpthread_mutex_t mutex;

long j;

void printnums() {
	long i;
	for (i = 0; i < 1000000000; i++) {
		rpthread_mutex_lock(&mutex);
		/*
		for (int k = 0; k < 100000000; k++) {
			k += 1 - 1;
			j += k - (k - 1);
		}
		for (int k = 0; k < 100000000; k++) {
			k += 1 - 1;
			j -= k - (k - 1);
		}
		for (int k = 0; k < 100000000; k++) {
			k += 1 - 1;
			j += k - (k - 1);
		}
		for (int k = 0; k < 100000000; k++) {
			k += 1 - 1;
			j -= k - (k - 1);
		}
		*/
		j++;
		rpthread_mutex_unlock(&mutex);
	}
}

int main(int argc, char **argv) {

	rpthread_t t1, t2;

	rpthread_mutex_init(&mutex, NULL);

	rpthread_create(&t1, NULL, printnums, NULL);

	rpthread_create(&t2, NULL, printnums, NULL);
	
	rpthread_join(t1, NULL);

	rpthread_join(t2, NULL);


	printf("Final j = %ld\n", j);

	return 0;
}
