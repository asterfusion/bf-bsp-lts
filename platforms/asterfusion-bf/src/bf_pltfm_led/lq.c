/*!
 * @file lq.c
 * @date 2021/07/01
 *
 * TSIHANG (tsihang@asterfusion.com)
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <assert.h>

#if defined(HAVE_BACKTRACE)
#include <execinfo.h>
#endif

#include "lq.h"

#define DO_LOCK(mtx)   \
    pthread_mutex_lock(mtx)
#define DO_UNLOCK(mtx) \
    pthread_mutex_unlock(mtx)

static void oryx_sys_mutex_create
(
    IO pthread_mutex_t *mtx
)
{
    int err;
    pthread_mutexattr_t a;

    err = pthread_mutexattr_init (&a);
    if (err) {
        fprintf (stdout,
                 "%s\n", oryx_safe_strerror (errno));
        exit (0);
    }

    err = pthread_mutexattr_settype (&a,
                                     PTHREAD_MUTEX_ERRORCHECK);
    if (err) {
        fprintf (stdout,
                 "%s\n", oryx_safe_strerror (errno));
        exit (0);
    }

    err = pthread_mutex_init (mtx, &a);
    if (err) {
        fprintf (stdout,
                 "%s\n", oryx_safe_strerror (errno));
        exit (0);
    }
}

#if defined(LQ_ENABLE_PASSIVE)
#define DO_COND_WAKE(c)\
    pthread_cond_signal(c)
#define DO_COND_WAITE(c,m)\
    pthread_cond_wait(c,m)

void oryx_sys_cond_create
(
    IN pthread_cond_t *c
)
{
    int err;

    err = pthread_cond_init (c, NULL);
    if (err) {
        fprintf (stdout,
                 "%s\n", oryx_safe_strerror (errno));
        exit (0);
    }
}

static void
_wakeup
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;
    DO_LOCK (&q->cond_mtx);
    DO_COND_WAKE (&q->cond);
    DO_UNLOCK (&q->cond_mtx);
}

static void
_hangon
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;
    if (lq_blocked_len (q) == 0) {
        DO_LOCK (&q->cond_mtx);
        DO_COND_WAITE (&q->cond, &q->cond_mtx);
        DO_UNLOCK (&q->cond_mtx);
    }
}
#endif

static struct oryx_lq_ctx_t *
    list_queue_init
(
    IN const char *fq_name,
    IN uint32_t fq_cfg,
    IN struct oryx_lq_ctx_t *lq
)
{
#if defined(BUILD_DEBUG)
    BUG_ON (lq == NULL);
#endif

    memset (lq, 0, sizeof (struct oryx_lq_ctx_t));
    lq->name        =   fq_name;
    lq->ul_flags    =   fq_cfg;
    lq->bot         =   NULL;
    lq->top         =   NULL;
    lq->len         =   0;
    oryx_sys_mutex_create (&lq->mtx);


#if defined(LQ_ENABLE_PASSIVE)
    //fprintf (stdout, "init passive queue... %d\n",
    //         lq_type_blocked (lq));
    if (lq_type_blocked (lq)) {
        oryx_sys_mutex_create (&lq->cond_mtx);
        oryx_sys_cond_create (&lq->cond);
        lq->fn_hangon   =   _hangon;
        lq->fn_wakeup   =   _wakeup;
    }
#endif

    return lq;
}

/**
 *  \brief Create a new queue
 *
 *  \param fq_name, the queue alias
 *  \param flags, configurations
 *  \param lq, the q
 */
int oryx_lq_new
(
    IN const char *fq_name,
    IN uint32_t fq_cfg,
    OUT void **lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              malloc (sizeof (struct oryx_lq_ctx_t));
    if (q == NULL) {
        fprintf (stdout,
                 "%s\n", oryx_safe_strerror (errno));
        exit (0);
    }
    (*lq) = list_queue_init (fq_name, fq_cfg, q);

    return 0;
}

/**
 *  \brief Destroy a queue
 *
 *  \param q the queue to destroy
 */
void oryx_lq_destroy
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;

    /* free all equeued elements. */

    /* destroy mutex */
    pthread_mutex_destroy (&q->mtx);

#if defined(LQ_ENABLE_PASSIVE)
    /* destroy cond mutex */
    pthread_mutex_destroy (&q->cond_mtx);
#endif
}

/**
 *  \brief Display a queue
 *
 *  \param q the queue to display
 */
void oryx_lq_dump
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;
    fprintf (stdout, "%16s%32s(%p)\n", "qname: ",
             q->name, lq);
    fprintf (stdout, "%16s%32d\n", "qlen: ",
             q->len);
    fprintf (stdout, "%16s%32d\n", "qcfg: ",
             q->ul_flags);
    fprintf (stdout, "%16s%32d\n", "delta: ",
             (int) (q->nr_eq_refcnt - q->nr_dq_refcnt));
}

/**
 *  \brief add an instance to a queue
 *
 *  \param q queue
 *  \param e instance
 */
void oryx_lq_enqueue
(
    IN void *lq,
    IN void *e
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;

#if defined(BUILD_DEBUG)
    BUG_ON (q == NULL || e == NULL);
#endif

    DO_LOCK (&q->mtx);

    if (q->top != NULL) {
        ((struct lq_prefix_t *)e)->lnext = q->top;
        ((struct lq_prefix_t *)q->top)->lprev = e;
        q->top = e;
    } else {
        q->top = e;
        q->bot = e;
    }

    q->len ++;
    q->nr_eq_refcnt ++;

#ifdef DBG_PERF
    if (q->len > q->dbg_maxlen) {
        q->dbg_maxlen = q->len;
    }
#endif /* DBG_PERF */

    DO_UNLOCK (&q->mtx);

#if defined(LQ_ENABLE_PASSIVE)
    if (lq_type_blocked (q)) {
        q->fn_wakeup (q);
    }
#endif

}

/**
 *  \brief remove an element from the list queue
 *
 *  \param q queue
 *
 *  \retval element or NULL if empty list.
 */
void *oryx_lq_dequeue
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;

#if defined(BUILD_DEBUG)
    BUG_ON (q == NULL);
#endif

#if defined(LQ_ENABLE_PASSIVE)
    if (lq_type_blocked (q)) {
        q->fn_hangon (q);
    }
#endif

    DO_LOCK (&q->mtx);

    void *e = q->bot;
    if (e == NULL) {
        DO_UNLOCK (&q->mtx);
        return NULL;
    }

    /* more elements in queue */
    if (((struct lq_prefix_t *)q->bot)->lprev !=
        NULL) {
        q->bot = ((struct lq_prefix_t *)q->bot)->lprev;
        ((struct lq_prefix_t *)q->bot)->lnext = NULL;
    } else {
        q->top = NULL;
        q->bot = NULL;
    }

    ((struct lq_prefix_t *)e)->lnext = NULL;
    ((struct lq_prefix_t *)e)->lprev = NULL;

#if defined(BUILD_DEBUG)
    BUG_ON (q->len == 0);
#endif
    q->nr_dq_refcnt ++;

    if (q->len > 0) {
        q->len--;
    }

    DO_UNLOCK (&q->mtx);

    return e;
}

int oryx_lq_length
(
    IN void *lq
)
{
    struct oryx_lq_ctx_t *q = (struct oryx_lq_ctx_t *)
                              lq;
    return q->len;
}
