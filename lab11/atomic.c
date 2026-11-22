/* atomic.c - the counter of race.c, with no lock at all: one atomic instruction per addition.
 * Build:  gcc -Wall -pthread -o atomic atomic.c
 */
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

#define N 1000000

atomic_long counter = 0;          /* every operation on it is indivisible */

void *add(void *arg) {
    for (int i = 0; i < N; i++)
        atomic_fetch_add(&counter, 1);   /* load, add, store as ONE step */
    return NULL;
}

int main(void) {
    pthread_t a, b;
    pthread_create(&a, NULL, add, NULL);
    pthread_create(&b, NULL, add, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("counter = %ld (expected %d)\n", (long)counter, 2 * N);
    return 0;
}
