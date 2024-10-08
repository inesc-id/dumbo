/* =============================================================================
 *
 * thread.c
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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include "thread.h"


#define MAXTHREADS 80
#define SIZE_HEAP 1048576

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t debug[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t ts_state[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t order_ts[MAXTHREADS];

__thread long ts_snapshot[80];
__thread long state_snapshot[80];

__attribute__((aligned(CACHE_LINE_SIZE))) uint64_t heap[SIZE_HEAP];
uint64_t *heappointer=heap;

__thread volatile uint64_t* mylogpointer;
__thread volatile uint64_t* mylogpointer_snapshot;
__thread volatile uint64_t* mylogend;
__thread volatile uint64_t* mylogstart;
__thread volatile long start_tx;
__thread volatile long end_tx;
__thread volatile long start_ts;
__thread volatile long end_ts;

uint64_t  **log_per_thread;
uint64_t  **log_pointer;
uint64_t  **log_replayer_start_ptr;
uint64_t  **log_replayer_end_ptr;

int global_order_ts=0;



/*__attribute__((aligned(CACHE_LINE_SIZE))) pthread_spinlock_t writers_lock = 0;

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t counters[80];

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t rot_counters[80];*/

//__thread readset_t* rot_readset;
/*__thread void* rot_readset[1000000];
__thread unsigned long rs_counter;

__thread unsigned long backoff = MIN_BACKOFF;
__thread unsigned long cm_seed = 123456789UL;

__attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[80];

*/__thread unsigned int thread_id;
/*__thread unsigned int local_thread_id;
__thread unsigned int local_exec_mode;*/

#ifndef REDUCED_TM_API

#include <assert.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include "types.h"
#include "random.h"
//#include "rapl.h"

static THREAD_LOCAL_T    global_threadId;
static THREAD_BARRIER_T* global_barrierPtr      = NULL;
static long*             global_threadIds       = NULL;
static THREAD_ATTR_T     global_threadAttr;
static THREAD_T*         global_threads         = NULL;
static void            (*global_funcPtr)(void*) = NULL;
static void*             global_argPtr          = NULL;
static volatile bool_t   global_doShutdown      = FALSE;

long              global_numThread;

static void
threadWait (void* argPtr)
{
    long threadId = *(long*)argPtr;

    THREAD_LOCAL_SET(global_threadId, (long)threadId);

    bindThread(threadId);

    thread_id = threadId;

    while (1) {
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for start parallel */
        if (global_doShutdown) {
            break;
        }
        global_funcPtr(global_argPtr);
        THREAD_BARRIER(global_barrierPtr, threadId); /* wait for end parallel */
        if (threadId == 0) {
            break;
        }
    }
}

/* =============================================================================
 * thread_startup
 * -- Create pool of secondary threads
 * -- numThread is total number of threads (primary + secondaries)
 * =============================================================================
 */
void
thread_startup (long numThread)
{
    int i;
    global_numThread = numThread;
    global_doShutdown = FALSE;


    /* Set up barrier */
    assert(global_barrierPtr == NULL);
    global_barrierPtr = THREAD_BARRIER_ALLOC(numThread);
    assert(global_barrierPtr);
    THREAD_BARRIER_INIT(global_barrierPtr, numThread);

    /* Set up ids */
    THREAD_LOCAL_INIT(global_threadId);
    assert(global_threadIds == NULL);
    global_threadIds = (long*)malloc(numThread * sizeof(long));
    assert(global_threadIds);
    for (i = 0; i < numThread; i++) {
        global_threadIds[i] = i;
    }

    /* Set up thread list */
    assert(global_threads == NULL);
    global_threads = (THREAD_T*)malloc(numThread * sizeof(THREAD_T));
    assert(global_threads);

    //writers_lock.value = 0;

    /* Set up pool */
    THREAD_ATTR_INIT(global_threadAttr);
    for (i = 1; i < numThread; i++) {
        THREAD_CREATE(global_threads[i],
                      global_threadAttr,
                      &threadWait,
                      &global_threadIds[i]);
    }
}

void
thread_start (void (*funcPtr)(void*), void* argPtr)
{
    global_funcPtr = funcPtr;
    global_argPtr = argPtr;

    long threadId = 0; /* primary */
    threadWait((void*)&threadId);
}


void
thread_shutdown ()
{
    /* Make secondary threads exit wait() */
    global_doShutdown = TRUE;
    THREAD_BARRIER(global_barrierPtr, 0);

    long numThread = global_numThread;

    long i;
    for (i = 1; i < numThread; i++) {
        THREAD_JOIN(global_threads[i]);
    }

	global_numThread = 1;

    THREAD_BARRIER_FREE(global_barrierPtr);
    global_barrierPtr = NULL;

    free(global_threadIds);
    global_threadIds = NULL;

    free(global_threads);
    global_threads = NULL;

}

barrier_t *barrier_alloc() {
    return (barrier_t *)malloc(sizeof(barrier_t));
}

void barrier_free(barrier_t *b) {
    free(b);
}

void barrier_init(barrier_t *b, int n) {
    pthread_cond_init(&b->complete, NULL);
    pthread_mutex_init(&b->mutex, NULL);
    b->count = n;
    b->crossing = 0;
}

void barrier_cross(barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    /* One more thread through */
    b->crossing++;
    /* If not all here, wait */
    if (b->crossing < b->count) {
        pthread_cond_wait(&b->complete, &b->mutex);
    } else {
        /* Reset for next time */
        b->crossing = 0;
        pthread_cond_broadcast(&b->complete);
    }
    pthread_mutex_unlock(&b->mutex);
}

void
thread_barrier_wait()
{
    long threadId = thread_getId();
    THREAD_BARRIER(global_barrierPtr, threadId);
}

long
thread_getId()
{
    return (long)THREAD_LOCAL_GET(global_threadId);
}

long
thread_getNumThread()
{
    return global_numThread;
}

/*inline void* TxLoad_P(void* var){
                        rot_readset[rs_counter] = var;
                        rs_counter++;
        return var;
}

inline long TxLoad(long var){
                rot_readset[rs_counter] = &var;
                rs_counter++;
        return var;
}*/



void
my_tm_startup(int numThread)
{
    log_per_thread= (uint64_t**)malloc(numThread*sizeof(uint64_t*));
    log_pointer= (uint64_t**)malloc(numThread*sizeof(uint64_t*));
    log_replayer_start_ptr= (uint64_t**)calloc(numThread,sizeof(uint64_t*));
    log_replayer_end_ptr= (uint64_t**)calloc(numThread,sizeof(uint64_t*));
    int i;
    for(i=0;i<numThread;i++){
        log_per_thread[i]=(uint64_t*)malloc(sizeof(uint64_t)*LOGSIZE);
        log_pointer[i]=log_per_thread[i];
        log_replayer_start_ptr[i]=log_per_thread[i];
        log_replayer_end_ptr[i]=log_per_thread[i];
    }
}
void replaylog();




void *log_replayer1234(void *a){
    //bindThread(16);
    //while(!global_doShutdown){
    //    replaylog();
    //}
    return NULL;
}
void
my_tm_thread_enter()
{
    mylogpointer=log_pointer[global_threadId];
    mylogend= mylogpointer+LOGSIZE-1;
    mylogstart= mylogpointer;
    if(thread_getId()==0){
       pthread_t t;
       if(pthread_create(&t,NULL,log_replayer1234,NULL)){
           perror("failure creating thread");
       };
    }
    assert(mylogpointer!=0 && "mylogpointer is null");\
}


// find the transaction with a smaller timestamp and replay it
void
replaylog(){
    uint64_t *log_start;
    uint64_t *log_end;
    uint64_t curTs;
    int curThread=-1;
    //initiates the curts with the biggest number(unsigned)
    curTs = (uint64_t)-1;
    int i;
  for (i = 0; i < global_numThread; ++i) {
      log_start=log_pointer[i];
      log_end=log_pointer[i]+LOGSIZE-1;
    // with concurrent multi replayers we need to check if curPtrs is ahead 
    uint64_t *start = log_replayer_start_ptr[i]; // logPtrs[i].log_ptrs.write_log_start;
    //printf("Log_start %lx\n", start);
    uint64_t *end = atomic_LOAD(log_replayer_end_ptr[i]);
    if (curThread == -1) curThread = i;
    if (start == end) {
     continue;
    } else {
      while (!isbit63one(*start) && start != end) {
        start++;
        if(start-log_end<=0){
            start=log_start;
        }
        start++;
        if(start-log_end<=0){
            start=log_start;
        }
      }
      uint64_t ts = zeroBit63(*start);
      if (ts<curTs) {
        curTs = ts;
        curThread = i;
      }
    }
  }
  //todo write the log transaction in thread cur thread into the persistant heap 
    if(curThread!=-1){
    log_start=log_pointer[curThread];
    log_end=log_pointer[curThread]+LOGSIZE-1;
    while (!isbit63one(*log_replayer_start_ptr[curThread])) {
        uint64_t location=((*log_replayer_start_ptr[curThread])/sizeof(uint64_t))%SIZE_HEAP;
        log_replayer_start_ptr[curThread]++;
        if(log_replayer_start_ptr[curThread]-log_end<=0){
            log_replayer_start_ptr[curThread]=log_start;
        }
        assert(location<SIZE_HEAP && "wrong location");
        uint64_t value=*log_replayer_start_ptr[curThread];
        heap[location]=value;
       // printf("log_replayer addr=%lx,value=%lx\n",location,value);
        log_replayer_start_ptr[curThread]++;
        if(log_replayer_start_ptr[curThread]-log_end<=0){
            log_replayer_start_ptr[curThread]=log_start;
        }
    }
   // printf("Pointer=%lx\n",log_replayer_start_ptr[curThread]);
  }

  //setting the start_ptr after the transaction
  if(curThread!=-1){
    log_start=log_pointer[curThread];
    log_end=log_pointer[curThread]+LOGSIZE-1;
    while (!isbit63one(*log_replayer_start_ptr[curThread])) {
        log_replayer_start_ptr[curThread]++;
        if(log_replayer_start_ptr[curThread]-log_end<=0){
            log_replayer_start_ptr[curThread]=log_start;
        }
        log_replayer_start_ptr[curThread]++;
        if(log_replayer_start_ptr[curThread]-log_end<=0){
            log_replayer_start_ptr[curThread]=log_start;
        }
      }
  }
  store_fence();
}

#endif
