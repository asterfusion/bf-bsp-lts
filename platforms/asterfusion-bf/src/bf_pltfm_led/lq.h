/*!
 * @file lq.h
 * @date 2021/07/01
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#ifndef LQ_H
#define LQ_H

/* Allow the use in C++ code.  */
#ifdef __cplusplus
extern "C" {
#endif

#include "pltfm_types.h"

#define BUILD_DEBUG
#define LQ_ENABLE_PASSIVE

/** add below to the beginning of your structure.
 *
 *  struct element_t {
 *      struct lq_prefix_t prefix;
 *      int data;
 *  };
 */
struct lq_prefix_t {
    void *lnext;    /* list */
    void *lprev;
};

/** trigger method. */
#define LQ_TYPE_PASSIVE (1 << 2)

/* all message stored in a fixed buffer (ul_cache_size).
 * if buffer full, new arrived element is dropped.
 * count for this case is record by ul_drop_refcnt. */
#define LQ_TYPE_FIXED_BUFFER    (1 << 3)

/* Define a queue for storing flows */
struct oryx_lq_ctx_t {
    void        *top,
                *bot;
    const char  *name;
    uint32_t    len;
    uint64_t    nr_eq_refcnt;
    uint64_t    nr_dq_refcnt;
#ifdef DBG_PERF
    uint32_t    dbg_maxlen;
#endif /* DBG_PERF */
    pthread_mutex_t mtx;

#if defined(LQ_ENABLE_PASSIVE)
    void (*fn_wakeup) (void *);
    void (*fn_hangon) (void *);
    pthread_mutex_t cond_mtx;
    pthread_cond_t  cond;
#endif
    int         unique_id;
    uint32_t    ul_flags;
};
//}__attribute__((__packed__));

#define lq_nr_eq(lq)        ((lq)->nr_eq_refcnt)
#define lq_nr_dq(lq)        ((lq)->nr_dq_refcnt)
#define lq_blocked_len(lq)  ((lq)->len)
#define lq_type_blocked(lq) ((lq)->ul_flags & LQ_TYPE_PASSIVE)

int oryx_lq_new
(
    IN const char *fq_name,
    IN uint32_t fq_cfg,
    OUT void **lq
);
void oryx_lq_destroy
(
    IN void *lq
);
void oryx_lq_dump
(
    IN void *lq
);
void oryx_lq_enqueue
(
    IN void *lq,
    IN void *e
);
void *oryx_lq_dequeue
(
    IN void *lq
);

int oryx_lq_length
(
    IN void *lq
);

#ifdef __cplusplus
}
#endif /* C++ */

#endif
