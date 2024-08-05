#include "replayer.hpp"
#include "replayer_internal.hpp"
#include <cstdlib>
#include <cassert>
#include <cstring>

static rep::log_entry_t * thread_logs;
static uint64_t * heap;
static rep::args_t g_args;

int rep::naive::init(rep::args_t &a)
{
  EASY_MALLOC(thread_logs, a.size_thread_log * a.nb_threads);
  memset(thread_logs, 0, a.size_thread_log * a.nb_threads * sizeof(rep::log_entry_t));
  EASY_MALLOC(heap, a.heap_size);
  g_args = a;
  return 0;
}

int rep::naive::destroy()
{
  free(thread_logs);
  free(heap);
  return 0;
}

int rep::naive::generate_log()
{
  long ts = 1; // for simplicity it uses a sequential clock, but the algorithm assumes physical TS
  
  rep::log_entry_t** ptr_l = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t** ptr_l_end = new rep::log_entry_t*[g_args.nb_threads];
  int nbWrTxs = 0;

  for (int i = 0; i < g_args.nb_threads; ++i)
  {
    ptr_l[i] = thread_logs + i * g_args.size_thread_log;
    ptr_l_end[i] = thread_logs + (i+1) * g_args.size_thread_log;
  }

  // fill the logs until some is filled
  while (1)
  {
    int t = rep::random(0, g_args.nb_threads - 1);
    int w = rep::random(g_args.min_writes, g_args.max_writes);

    if (ptr_l[t] + w >= ptr_l_end[t])
      break; // thread t is full: stop

    ptr_l[t][w].is_commit = onesBit63(ts);
    ptr_l[t][w].ts = ts; // TODO: we are wasting 64 bits
    ts++;

    // adds each memory position (starts from the end of the log)
    while (w-- > 0)
    {
      long p = rep::random_access();
      ptr_l[t]->addr = (uint64_t) (heap + p);
      ptr_l[t]->val = p;
      ptr_l[t]++;
    }

    // TODO: this implementation use 2 slots (addr+val) for the TS
    // the final commit marker already filled
    ptr_l[t]++;
    nbWrTxs++;
  }

  delete [] ptr_l;
  delete [] ptr_l_end;
  return nbWrTxs;
}


int rep::naive::replay()
{
  int nbReps = 0;
  // long nbWrites = 0;

  rep::log_entry_t** ptr_l = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t** ptr_l_end = new rep::log_entry_t*[g_args.nb_threads];
  uint64_t* sort_ts = new uint64_t[g_args.nb_threads];
  uint64_t tsToRep = (uint64_t)-1; // initiates tsToRep with the biggest number (unsigned)

  for (int i = 0; i < g_args.nb_threads; ++i)
  {
    ptr_l[i] = thread_logs + i * g_args.size_thread_log;
    ptr_l_end[i] = thread_logs + (i+1) * g_args.size_thread_log;
    sort_ts[i] = 0;
  }

  while (1)
  {
    int threadToRep = -1;
    rep::log_entry_t *log_start;
    rep::log_entry_t *log_end;

    // find the next thread to replay
    for ( int i = 0; i < g_args.nb_threads; ++i )
    {
      log_start = ptr_l[i];
      log_end = ptr_l_end[i];
      
      if (ptr_l[i] >= ptr_l_end[i] || ptr_l[i]->addr == 0)
        continue;

      if (sort_ts[i] != 0 && sort_ts[i] < tsToRep)
      {
        // use cached TS instead of sweep the log again
        tsToRep = sort_ts[i];
        threadToRep = i;
        continue;
      }

      rep::log_entry_t *start = ptr_l[i];

      if (threadToRep == -1)
        threadToRep = i;
      else // check if this thread has a smaller TS than threadToRep
      {
        while ( !isbit63one(start->is_commit) ) // assumes well formed log
        {
          assert(start <= log_end && start->addr != 0 && "Log is not well formed");
          start++; // Jumps addr+val to the addr in the next position
          // TODO: no wrap-arounds implemented
        }

        uint64_t ts = zeroBit63(start->is_commit); // TODO: start->ts is wasted
        if ( ts < tsToRep )
        {
          tsToRep = ts;
          sort_ts[i] = ts;
          threadToRep = i;
        }
      }
    }

    if (threadToRep == -1)
      break; // there is nothing else left to replay

    sort_ts[threadToRep] = 0; // needs to sweep again in the next look up

    // replay the next thread
    log_start = ptr_l[threadToRep];
    log_end = ptr_l_end[threadToRep];
    while (!isbit63one(log_start->is_commit))
    {
      ptr_l[threadToRep]++; // jumps addr+val (type of ptr_l is a log_entry not uint64_t)
      
      // checks if the address is valid
      assert(log_start->addr - (uint64_t)heap >= 0 && log_start->addr - (uint64_t)heap < sizeof(uint64_t) * g_args.heap_size && "Wrong Addr");
      
      // writes back to PM
      *((uint64_t*)log_start->addr) = log_start->val;
      flush((uint64_t*)log_start->addr);

      log_start++; // jumps addr+val
      // nbWrites++;
      // TODO: no wrap-arounds implemented
    }
    ptr_l[threadToRep]++; // jumps the TS
    nbReps++;
    // TODO: the replayer may want to truncate log space to unblock workers doing write transactions
    // emulates flush of metadata that flags the workers that there is more log space
    // flush((uint64_t*)log_start);
    flush_barrier();
  }

  // printf("nbWrites = %li\n", nbWrites);

  delete [] ptr_l;
  delete [] ptr_l_end;
  delete [] sort_ts;
  return nbReps;
}


