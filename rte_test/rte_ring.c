#include "rte_ring.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef __GNUC__
#define likely(x)    __builtin_expect(!!(x), 1)
#define unlikely(x)  __builtin_expect(!!(x), 0)
#else
#define likely(x)    (x)
#define unlikely(x)  (x)
#endif

#ifdef __AARCH64EL__
__attribute__((always_inline)) inline void _mm_pause() { __asm__ __volatile__("yield"); }
#else
#include <emmintrin.h>
#endif

static inline void rte_pause(void) { _mm_pause(); }
static inline int rte_atomic32_cmpset(volatile uint32_t *dst, uint32_t exp, uint32_t src) {
    return __sync_bool_compare_and_swap(dst, exp, src);
}
static inline void rte_barrier() { __sync_synchronize(); }

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

struct rte_ring *rte_ring_create(unsigned count, unsigned flags) {
    struct rte_ring *r;

    if (!POWEROF2(count)) return NULL;
    r = (struct rte_ring *)malloc(sizeof(struct rte_ring) + count*sizeof(void *));
    if (r == NULL) return NULL;

    r->prod.size = r->cons.size = count;
    r->prod.head = r->cons.head = 0;
    r->prod.tail = r->cons.tail = 0;
    r->prod.sp_enqueue = flags & RING_F_SP_ENQ;
    r->cons.sc_dequeue = flags & RING_F_SC_DEQ;
    return r;
}

void rte_ring_free(struct rte_ring *r, void (*obj_free)(void *)) {
    /* free obj in rte_ring array at first */
    if (obj_free) {
        void *obj;
        while (rte_ring_dequeue(r, &obj) > 0) obj_free(obj);
    }
    free(r);
}

/* Enqueue one objects on a ring (NOT multi-producers safe). */
static inline int __attribute__((always_inline))
__rte_ring_sp_enqueue(struct rte_ring *r, void *reply) {
    uint32_t prod_head, prod_next;
    uint32_t cons_tail, free_entries;
    uint32_t mask = r->prod.size-1;

    prod_head = r->prod.head;
    cons_tail = r->cons.tail;

    free_entries = (mask + cons_tail - prod_head);
    if (unlikely(1 > free_entries))
        return 0;

    prod_next = prod_head + 1;
    r->prod.head = prod_next;
    r->ring[prod_head&mask] = reply;

    rte_barrier();

    r->prod.tail = prod_next;
// #if defined(__arm__) || defined(__aarch64__)
//     rte_barrier();
// #endif
    return 1;
}

/* Enqueue one objects on a ring (multi-producers safe). */
static inline int __attribute__((always_inline))
__rte_ring_mp_enqueue(struct rte_ring *r, void *reply) {
    uint32_t prod_head, prod_next;
    uint32_t cons_tail, free_entries;
    int success;
    uint32_t mask = r->prod.size-1;

    do {
        prod_head = r->prod.head;
        cons_tail = r->cons.tail;

        free_entries = (mask + cons_tail - prod_head);
        if (unlikely(1 > free_entries))
            return 0;
        prod_next = prod_head + 1;
        success = rte_atomic32_cmpset(&r->prod.head, prod_head, prod_next);
    } while (unlikely(success == 0));

    r->ring[prod_head&mask] = reply;

    rte_barrier();
    while (unlikely(r->prod.tail != prod_head))
        rte_pause();

    r->prod.tail = prod_next;
// #if defined(__arm__) || defined(__aarch64__)
//     rte_barrier();
// #endif
    return 1;
}

/* Dequeue one objects from a ring (NOT multi-consumers safe). */
static inline int __attribute__((always_inline))
__rte_ring_sc_dequeue(struct rte_ring *r, void **reply) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    uint32_t mask = r->prod.size-1;

    cons_head = r->cons.head;
    prod_tail = r->prod.tail;

    entries = prod_tail - cons_head;
    if (unlikely(1 > entries))
        return 0;

    cons_next = cons_head + 1;
    r->cons.head = cons_next;
    *reply = r->ring[cons_head & mask];

    rte_barrier();

    r->cons.tail = cons_next;
    return 1;
}

/* Dequeue one objects from a ring (NOT multi-consumers safe). */
static inline int __attribute__((always_inline))
__rte_ring_mc_dequeue(struct rte_ring *r, void **reply) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    uint32_t mask = r->prod.size-1;
    int success;

    do {
        cons_head = r->cons.head;
        prod_tail = r->prod.tail;

        entries = prod_tail - cons_head;
        if (unlikely(1 > entries))
            return 0;

        cons_next = cons_head + 1;
        success = rte_atomic32_cmpset(&r->cons.head, cons_head, cons_next);
    } while (unlikely(success == 0));

    *reply = r->ring[cons_head & mask];

    rte_barrier();
    while (unlikely(r->cons.tail != cons_head))
        rte_pause();

    r->cons.tail = cons_next;
    return 1;
}

int rte_ring_enqueue(struct rte_ring *r, void *reply) {
    if (r->prod.sp_enqueue)
        return __rte_ring_sp_enqueue(r, reply);
    return __rte_ring_mp_enqueue(r, reply);
}

int rte_ring_dequeue(struct rte_ring *r, void **reply) {
    if (r->cons.sc_dequeue)
        return __rte_ring_sc_dequeue(r, reply);
    return __rte_ring_mc_dequeue(r, reply);
}

unsigned rte_ring_count(const struct rte_ring *r) {
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    return ((prod_tail - cons_tail) & (r->prod.size-1));
}

unsigned rte_ring_free_count(const struct rte_ring *r) {
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    return ((cons_tail - prod_tail - 1) & (r->prod.size-1));
}

int rte_ring_full(const struct rte_ring *r) {
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    return (((cons_tail - prod_tail - 1) & (r->prod.size-1)) == 0);
}

int rte_ring_empty(const struct rte_ring *r) {
    uint32_t prod_tail = r->prod.tail;
    uint32_t cons_tail = r->cons.tail;
    return !!(cons_tail == prod_tail);
}
