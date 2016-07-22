#ifndef _POOL_H_
#define _POOL_H_

#ifndef POOL_DEBUG
#define POOL_DEBUG
#endif

#ifndef NULL
#define NULL 0
#endif

/** 
 * pool_cleanup_t - callback type which is associated
 * with a pool entry; invoked when the pool entry is 
 * free'd 
 **/
typedef void (*pool_cleanup_t)(void *arg);

/** 
 * pheap - singular allocation of memory 
 **/
struct pheap
{
    void *block;
    int size, used;
};

/** 
 * pfree - a linked list node which stores an
 * allocation chunk, plus a callback 
 **/
struct pfree
{
    pool_cleanup_t f;
    void *arg;
    struct pheap *heap;
    struct pfree *next;
};

/**
 * pool - base node for a pool. Maintains a linked list
 * of pool entries (pfree)
 **/
typedef struct pool_struct
{
    int size;
    struct pfree *cleanup;
    struct pfree *cleanup_tail;
    struct pheap *heap;
#ifdef POOL_DEBUG
    char name[8], zone[32];
    int lsize;
#endif
} _pool, *pool_t;

#ifdef POOL_DEBUG
# define pool_new() _pool_new(__FILE__,__LINE__) 
# define pool_heap(i) _pool_new_heap(i,__FILE__,__LINE__) 
#else
# define pool_heap(i) _pool_new_heap(i,NULL,0) 
# define pool_new() _pool_new(NULL,0)
#endif

pool_t _pool_new(const char *file, int line); /* new pool :) */
pool_t _pool_new_heap(int size, const char *file, int line); /* creates a new memory pool with an initial heap size */
void *pmalloc(pool_t, int size); /* wrapper around malloc, takes from the pool, cleaned up automatically */
void *pmalloc_x(pool_t p, int size, char c); /* Wrapper around pmalloc which prefils buffer with c */
void *pmalloco(pool_t p, int size); /* YAPW for zeroing the block */
char *pstrdup(pool_t p, const char *src); /* wrapper around strdup, gains mem from pool */
char *pstrdupx(pool_t p, const char *src, int len); /* use given len */
void pool_stat(int full); /* print to stderr the changed pools and reset */
void pool_cleanup(pool_t p, pool_cleanup_t fn, void *arg); /* calls f(arg) before the pool is freed during cleanup */
void pool_free(pool_t p); /* calls the cleanup functions, frees all the data on the pool, and deletes the pool itself */
int pool_size(pool_t p); /* returns total bytes allocated in this pool */

#endif

