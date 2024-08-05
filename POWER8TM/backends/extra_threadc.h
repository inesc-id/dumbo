#include "tm.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "seq_log.h"

#define MAXTHREADS 80
#define SIZE_HEAP 1048576

__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t debug[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t ts_state[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t dur_state[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t order_ts[MAXTHREADS];
__attribute__((aligned(CACHE_LINE_SIZE))) __thread tx_local_vars_t loc_var;
__attribute__((aligned(CACHE_LINE_SIZE))) __thread QUIESCENCE_CALL_ARGS_t q_args;
__attribute__((aligned(CACHE_LINE_SIZE))) padded_scalar_t max_cache_line[80];

__thread __attribute__((aligned(CACHE_LINE_SIZE))) long ts_snapshot[80];
__thread __attribute__((aligned(CACHE_LINE_SIZE))) long state_snapshot[80];

__attribute__((aligned(CACHE_LINE_SIZE))) uint64_t heap[SIZE_HEAP];
uint64_t *heappointer=heap;

__thread unsigned int local_thread_id;
__thread unsigned int local_exec_mode;

__thread volatile long start_tx;
__thread volatile long end_tx;
__thread volatile long start_ts;
__thread volatile long end_ts;
__thread volatile long start_sus;
__thread volatile long end_sus;
__thread volatile long start_flush;
__thread volatile long end_flush;
// __thread volatile long start_wait2;
// __thread volatile long end_wait2;


uint64_t  **log_per_thread;
uint64_t  **log_pointer;
uint64_t  **log_replayer_start_ptr;
uint64_t  **log_replayer_end_ptr;

__attribute__((aligned(CACHE_LINE_SIZE))) int single_global_lock;
__attribute__((aligned(CACHE_LINE_SIZE))) int global_order_ts = 0;
__attribute__((aligned(CACHE_LINE_SIZE))) int place_abort_marker = 1;
__thread unsigned long rs_counter;
__attribute__((aligned(CACHE_LINE_SIZE))) padded_statistics_t stats_array[80];

seql_node_t *seql_global_ptr; // seq_log.h

void
my_tm_startup(int numThread)
{
  log_per_thread = (uint64_t**)malloc(numThread*sizeof(uint64_t*));
  log_pointer = (uint64_t**)malloc(numThread*sizeof(uint64_t*));
  log_replayer_start_ptr = (uint64_t**)calloc(numThread,sizeof(uint64_t*));
  log_replayer_end_ptr = (uint64_t**)calloc(numThread,sizeof(uint64_t*));
  int i;
  for ( i = 0; i < numThread; i++ )
  {
    log_per_thread[i] = (uint64_t*)malloc(sizeof(uint64_t)*LOGSIZE);
    log_pointer[i] = log_per_thread[i];
    log_replayer_start_ptr[i] = log_per_thread[i];
    log_replayer_end_ptr[i] = log_per_thread[i];
  }
}

void replaylog();

void *
log_replayer1234(void *a)
{
    //bindThread(16);
    //while(!global_doShutdown){
    //    replaylog();
    //}
    return NULL;
}

void
my_tm_thread_enter()
{
  loc_var.mylogpointer = log_pointer[global_threadId];
  loc_var.mylogend = loc_var.mylogpointer + LOGSIZE - 1;
  loc_var.mylogstart = loc_var.mylogpointer;
  if ( thread_getId() == 0 )
  {
    pthread_t t;
    if ( pthread_create(&t, NULL, log_replayer1234, NULL) )
    {
      perror("failure creating thread");
    }
  }
  assert(loc_var.mylogpointer!=0 && "mylogpointer is null");\
}


// find the transaction with a smaller timestamp and replay it
void
replaylog()
{
  uint64_t *log_start;
  uint64_t *log_end;
  uint64_t curTs;
  int curThread = -1;
  //initiates the curts with the biggest number(unsigned)
  curTs = (uint64_t)-1;
  int i;
  for ( i = 0; i < global_numThread; ++i )
  {
    log_start = log_pointer[i];
    log_end = log_pointer[i] + LOGSIZE - 1;
    // with concurrent multi replayers we need to check if curPtrs is ahead 
    uint64_t *start = log_replayer_start_ptr[i]; // logPtrs[i].log_ptrs.write_log_start;
    //printf("Log_start %lx\n", start);
    uint64_t *end = atomic_LOAD(log_replayer_end_ptr[i]);
    if (curThread == -1)
      curThread = i;
    if (start == end)
     continue;
    else
    {
      while ( !isbit63one(*start) && start != end )
      {
        start++;
        if ( start-log_end <= 0 )
        {
          start=log_start;
        }
        start++;
        if ( start-log_end <= 0 )
        {
          start=log_start;
        }
      }
      uint64_t ts = zeroBit63(*start);
      if ( ts < curTs )
      {
        curTs = ts;
        curThread = i;
      }
    }
  }
  //todo write the log transaction in thread cur thread into the persistant heap 
  if ( curThread != -1 )
  {
    log_start = log_pointer[curThread];
    log_end = log_pointer[curThread] + LOGSIZE-1;
    while (!isbit63one(*log_replayer_start_ptr[curThread]))
    {
      uint64_t location = ((*log_replayer_start_ptr[curThread]) / sizeof(uint64_t)) % SIZE_HEAP;
      log_replayer_start_ptr[curThread]++;
      if ( log_replayer_start_ptr[curThread] - log_end <= 0 )
      {
        log_replayer_start_ptr[curThread] = log_start;
      }
      assert(location<SIZE_HEAP && "wrong location");
      uint64_t value = *log_replayer_start_ptr[curThread];
      heap[location] = value;
      // printf("log_replayer addr=%lx,value=%lx\n",location,value);
      log_replayer_start_ptr[curThread]++;
      if ( log_replayer_start_ptr[curThread] - log_end <= 0 )
      {
        log_replayer_start_ptr[curThread]=log_start;
      }
    }
   // printf("Pointer=%lx\n",log_replayer_start_ptr[curThread]);
  }

  //setting the start_ptr after the transaction
  if ( curThread != -1 )
  {
    log_start=log_pointer[curThread];
    log_end=log_pointer[curThread] + LOGSIZE - 1;
    while ( !isbit63one(*log_replayer_start_ptr[curThread]) )
    {
      log_replayer_start_ptr[curThread]++;
      if ( log_replayer_start_ptr[curThread] - log_end <= 0 )
      {
        log_replayer_start_ptr[curThread] = log_start;
      }
      log_replayer_start_ptr[curThread]++;
      if ( log_replayer_start_ptr[curThread] - log_end <= 0 )
      {
        log_replayer_start_ptr[curThread] = log_start;
      }
    }
  }
  store_fence();
}

#ifdef __cplusplus
}
#endif
