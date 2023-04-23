#ifndef _RTE_RING_H_
#define _RTE_RING_H_

#include <stdint.h>

#define CACHE_LINE_SIZE 64
#define CACHE_LINE_MASK (CACHE_LINE_SIZE-1)

#define RING_F_SP_ENQ   0x01u
#define RING_F_SC_DEQ   0x02u

#define __rte_cache_aligned __attribute__((__aligned__(CACHE_LINE_SIZE)))

struct rte_ring {
    struct prod {
        volatile uint32_t head;         /* producer head */
        volatile uint32_t tail;         /* producer tail */
        uint32_t          size;         /* size of producer ring */
        uint32_t          sp_enqueue;   /* True, if single producer */
    } prod __rte_cache_aligned;

    struct cons {
        volatile uint32_t head;         /* consumer head */
        volatile uint32_t tail;         /* consumer tail */
        uint32_t          size;         /* size of consumer ring */
        uint32_t          sc_dequeue;   /* True, if single consumer */
    } cons __rte_cache_aligned;

    void *ring[] __rte_cache_aligned;   /* memory space of ring starts */
};

struct rte_ring *rte_ring_create(unsigned count, unsigned flags);
void rte_ring_free(struct rte_ring *r, void (*obj_free)(void *));
int rte_ring_enqueue(struct rte_ring *r, void *reply);
int rte_ring_dequeue(struct rte_ring *r, void **reply);
int rte_ring_empty(const struct rte_ring *r);
int rte_ring_full(const struct rte_ring *r);
unsigned rte_ring_free_count(const struct rte_ring *r);
unsigned rte_ring_count(const struct rte_ring *r);

#endif
