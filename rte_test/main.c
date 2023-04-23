#include "rte_ring.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Implementation using __atomic macros. */
#define atomicIncr(var,count) __atomic_add_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomicGetIncr(var,oldvalue_var,count) do { \
    oldvalue_var = __atomic_fetch_add(&var,(count),__ATOMIC_RELAXED); \
} while(0)
#define atomicDecr(var,count) __atomic_sub_fetch(&var,(count),__ATOMIC_RELAXED)
#define atomicGet(var,dstvar) do { \
    dstvar = __atomic_load_n(&var,__ATOMIC_RELAXED); \
} while(0)
#define atomicSet(var,value) __atomic_store_n(&var,value,__ATOMIC_RELAXED)


int g_count = 0;
int g_per_thread_enc_num = 100000;
#define THREAD_NUM 4


void* doEnqueue(void *private) {
    struct rte_ring *r = (struct rte_ring*)private;
    for (int i = 0; i < g_per_thread_enc_num; i++) {
        rte_ring_enqueue(r, (void *)i);
    }
    atomicDecr(g_count, 1);
    return NULL;
}

int main() {
    struct rte_ring *r = rte_ring_create(1<<20UL, RING_F_SC_DEQ);
    pthread_t thread_id[THREAD_NUM];
    for (int i = 0; i < THREAD_NUM; i++) {
        
        int res = pthread_create(&thread_id[i], NULL, doEnqueue, (void *)r);
        if (res != 0) {
            printf("create thread err\n");
            exit(1);
        }
    }
    atomicIncr(g_count, 4);
    for (;;) {
        int cur_count;
        atomicGet(g_count, cur_count);
        if (cur_count == 0) {
            break;
        }
    }
   
    int final_size = rte_ring_count(r);
    printf("ret count: %d\n", final_size);
    assert(final_size == g_per_thread_enc_num * THREAD_NUM);

    return 0;
}