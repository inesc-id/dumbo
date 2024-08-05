/* =============================================================================
 *
 * thread.h
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of ssca2, please see ssca2/COPYRIGHT
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#ifndef THREAD_H
#define THREAD_H 1

# define CACHE_LINE_SIZE 128

#include <pthread.h>
#include <stdint.h>
//#include "timer.h"

#define __dcbst(base, index)    \
  __asm__ ("dcbst %0, %1" : /*no result*/ : "b%" (index), "r" (base) : "memory")

typedef struct padded_statistics {
    unsigned long total_time;
    unsigned long wait_time;
    unsigned long read_commits;
    unsigned long htm_commits;
    unsigned long htm_conflict_aborts;
    unsigned long htm_self_conflicts;
    unsigned long htm_trans_conflicts;
    unsigned long htm_nontrans_conflicts;
    unsigned long htm_user_aborts;
    unsigned long htm_capacity_aborts;
    unsigned long htm_persistent_aborts;
    unsigned long htm_other_aborts;
    unsigned long rot_commits;
    unsigned long rot_conflict_aborts;
    unsigned long rot_self_conflicts;
    unsigned long rot_trans_conflicts;
    unsigned long rot_nontrans_conflicts;
    unsigned long rot_other_conflicts;
    unsigned long rot_user_aborts;
    unsigned long rot_persistent_aborts;
    unsigned long rot_capacity_aborts;
    unsigned long rot_other_aborts;
    unsigned long gl_commits;
    char suffixPadding[CACHE_LINE_SIZE];
} __attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t;

typedef struct readset_item {
        long* addr;
	long addr_p;
	int type;
        struct readset_item *next;
} readset_item_t;

typedef struct readset {
        readset_item_t *head;
} readset_t;

// -------------------------------
//   state[tid] ←  active
//   StartTS[tid] ←  currentTime()
//   OrderTS[tid] ←  -1;
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t debug[];
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t start_ts[];
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t start_ts_snapshot[];
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t end_ts[];
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t order_ts[];
extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t not_durable[];
extern uint64_t  **log_per_thread;
extern uint64_t  **log_pointer;
extern int global_order_ts;
extern __thread uint64_t* mylogpointer;
extern __thread uint64_t* mylogpointer_snapshot;
extern __thread uint64_t* mylogend;
extern __thread uint64_t* mylogstart;
extern uint64_t  **log_replayer_start_ptr;
extern uint64_t  **log_replayer_end_ptr;
// -------------------------------

#define LOGSIZE 400000
#define MIN_SPACE_LOG 200000

extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t counters[];

extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t triggers[];

extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t rot_counters[];

//extern __thread readset_t* rot_readset;

extern __thread long rot_readset_values[];
extern __thread void* rot_readset[];
extern __thread unsigned long rs_counter;

extern __thread unsigned long backoff;
extern __thread unsigned long cm_seed;
//extern __thread TIMER_T start_time;

# ifndef MIN_BACKOFF
#  define MIN_BACKOFF                   (1UL << 2)
# endif /* MIN_BACKOFF */
# ifndef MAX_BACKOFF
#  define MAX_BACKOFF                   (1UL << 31)
# endif /* MAX_BACKOFF */

extern __attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t single_global_lock;

extern __attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[];

extern __thread long counters_snapshot[80];
extern long total_first_time;
extern long total_second_time;


extern long              global_numThread;
//static long global_numThread = 1;

extern __thread unsigned int local_thread_id;

extern __thread unsigned int local_exec_mode;

#ifndef REDUCED_TM_API

#include <stdlib.h>
#include <assert.h>
#include "types.h"



#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------
#define bit63one 0x8000000000000000lu
#define isbit63one(ts) ((ts & bit63one)==bit63one)
#define zeroBit63(ts)  (ts & ~bit63one)
#define store_fence() __atomic_thread_fence(__ATOMIC_RELEASE)
#define load_fence() __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_STORE(ptr,val) __atomic_store_n(&(ptr), val, __ATOMIC_RELEASE)
#define atomic_LOAD(ptr) __atomic_load_n(&(ptr), __ATOMIC_ACQUIRE)

void
my_tm_startup(int numThread);

void
my_tm_thread_enter();







// ptr pointer for the log (uint64_t*)
// addr address to write
// val value to write
// start the start of the log
// end the end of the log
// todo double check cast doubles to int
#define write_in_log(ptr,addr,val,start,end)\
*ptr=(uint64_t)addr;\
ptr++;\
if(end-ptr<=0){\
    ptr=start;\
}\
*ptr=val;\
ptr++;\
if(end-ptr<=0){\
    ptr=start;\
}\
assert(ptr-end<=0 && "ptr is larger than end of the log");\

# define emulate_pm_slowdown(){\
    for(volatile int i=0;i<1000;i++);\
}\


//todo check size of cache line size, flush only cache line and not every entry of the  log_cache_line/8bytes=entrerys in log that fit in cache line //////i<8?
#define commit_log(ptr,ts,start,end)\
    while(mylogpointer_snapshot!=ptr){ \
        __dcbst(mylogpointer_snapshot,0); \
        emulate_pm_slowdown();\
        /*advance one cacheline */\
        for(int _i=0;_i<16;_i++){ \
            mylogpointer_snapshot++;\
            if(end-mylogpointer_snapshot<=0){\
                mylogpointer_snapshot=start;\
            }\
        }\
    } \


#define commit_log_marker(ptr,ts,start,end)\
*ptr=bit63one | ts;\
__dcbst(ptr,0); \
ptr++;\
if(ptr-end<=0){\
    ptr=start;\
}\
atomic_STORE(log_replayer_end_ptr[local_thread_id],ptr);\





//------------------------------------

#define THREAD_T                            pthread_t
#define THREAD_ATTR_T                       pthread_attr_t

#define THREAD_ATTR_INIT(attr)              pthread_attr_init(&attr)
#define THREAD_JOIN(tid)                    pthread_join(tid, (void**)NULL)
#define THREAD_CREATE(tid, attr, fn, arg)   pthread_create(&(tid), \
                                                           &(attr), \
                                                           (void* (*)(void*))(fn), \
                                                           (void*)(arg))

#define THREAD_LOCAL_T                      pthread_key_t
#define THREAD_LOCAL_INIT(key)              pthread_key_create(&key, NULL)
#define THREAD_LOCAL_SET(key, val)          pthread_setspecific(key, (void*)(val))
#define THREAD_LOCAL_GET(key)               pthread_getspecific(key)

#define THREAD_MUTEX_T                      pthread_mutex_t
#define THREAD_MUTEX_INIT(lock)             pthread_mutex_init(&(lock), NULL)
#define THREAD_MUTEX_LOCK(lock)             pthread_mutex_lock(&(lock))
#define THREAD_MUTEX_UNLOCK(lock)           pthread_mutex_unlock(&(lock))

#define THREAD_COND_T                       pthread_cond_t
#define THREAD_COND_INIT(cond)              pthread_cond_init(&(cond), NULL)
#define THREAD_COND_SIGNAL(cond)            pthread_cond_signal(&(cond))
#define THREAD_COND_BROADCAST(cond)         pthread_cond_broadcast(&(cond))
#define THREAD_COND_WAIT(cond, lock)        pthread_cond_wait(&(cond), &(lock))

#  define THREAD_BARRIER_T                  barrier_t
#  define THREAD_BARRIER_ALLOC(N)           barrier_alloc()
#  define THREAD_BARRIER_INIT(bar, N)       barrier_init(bar, N)
#  define THREAD_BARRIER(bar, tid)          barrier_cross(bar)
#  define THREAD_BARRIER_FREE(bar)          barrier_free(bar)

typedef struct barrier {
    pthread_cond_t complete;
    pthread_mutex_t mutex;
    int count;
    int crossing;
} barrier_t;

barrier_t *barrier_alloc();

void barrier_free(barrier_t *b);

void barrier_init(barrier_t *b, int n);

void barrier_cross(barrier_t *b);

void thread_startup (long numThread);

void thread_start (void (*funcPtr)(void*), void* argPtr);

void thread_shutdown ();

void thread_barrier_wait();

long thread_getId();

long thread_getNumThread();


#ifdef __cplusplus
}
#endif

#endif

#endif /* THREAD_H */


/* =============================================================================
 *
 * End of thread.h
 *
 * =============================================================================
 */
