#include "replayer.hpp"
#include "replayer_internal.hpp"
#include <cstdlib>
#include <cassert>
#include <cstring>

static rep::log_entry_t * thread_logs;
static uint64_t * heap;
static rep::args_t g_args;

int rep::forward_link::init(rep::args_t &a)
{
  EASY_MALLOC(thread_logs, a.size_thread_log * a.nb_threads);
  memset(thread_logs, 0, a.size_thread_log * a.nb_threads * sizeof(rep::log_entry_t));
  EASY_MALLOC(heap, a.heap_size);
  g_args = a;
  return 0;
}

int rep::forward_link::destroy()
{
  free(thread_logs);
  free(heap);
  return 0;
}

int rep::forward_link::generate_log()
{
  long ts = 1; // for simplicity it uses a sequential clock, but the algorithm assumes physical TS
  
  rep::log_entry_t** ptr_l = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t** ptr_l_end = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t* link_pos = nullptr;
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

    if (link_pos) // this needs to come after the break
      link_pos->link_next = ptr_l[t];

    link_pos = &(ptr_l[t][w]); // next TX needs to write its position here
    ptr_l[t][w].commit_ts = onesBit63(ts);
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

int rep::forward_link::replay()
{
  int nbReps = 0;

  rep::log_entry_t** ptr_l = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t** ptr_l_end = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t *log_start;
  rep::log_entry_t *log_end;
  rep::log_entry_t link_start;
  rep::log_entry_t* link_pos = &link_start; // first position out of the log
  uint64_t tsToRep = (uint64_t)-1; // initiates tsToRep with the biggest number (unsigned)
  // long nbWrites = 0;

  link_start.commit_ts = 0;
  link_start.link_next = nullptr;

  for (int i = 0; i < g_args.nb_threads; ++i)
  {
    ptr_l[i] = thread_logs + i * g_args.size_thread_log;
    ptr_l_end[i] = thread_logs + (i+1) * g_args.size_thread_log;
  }

  // find the first TX and follow the linkgs afterwards
  for ( int i = 0; i < g_args.nb_threads; ++i )
  {
    log_start = ptr_l[i];
    log_end = ptr_l_end[i];
    
    if (log_start >= log_end || log_start->addr == 0)
      continue;

    rep::log_entry_t *start = log_start;

    while ( !isbit63one(start->commit_ts) ) // assumes well formed log
    {
      assert(start <= log_end && start->addr != 0 && "Log is not well formed");
      start++; // Jumps addr+val to the addr in the next position
      // TODO: no wrap-arounds implemented
    }

    uint64_t ts = zeroBit63(start->commit_ts);
    if ( ts < tsToRep )
    {
      tsToRep = ts;
      link_start.link_next = log_start;
    }
  }

  while (1)
  {
    // replay the next thread
    log_start = link_pos->link_next;

    if (!log_start)
      break; // there is nothing else left to replay
    
    // TODO: I think the last TX is not being replayed
    while (!isbit63one(log_start->commit_ts))
    {
      // checks if the address is valid
      assert(log_start->addr - (uint64_t)heap >= 0 && log_start->addr - (uint64_t)heap < sizeof(uint64_t) * g_args.heap_size && "Wrong Addr");
      
      // writes back to PM
      *((uint64_t*)log_start->addr) = log_start->val;
      flush((uint64_t*)log_start->addr);

      log_start++; // jumps addr+val
      // TODO: no wrap-arounds implemented
      // nbWrites++;
    }
    link_pos = log_start; // next tx
    nbReps++;

    // TODO: the replayer may want to truncate log space to unblock workers doing write transactions
    // emulates flush of metadata that flags the workers that there is more log space
    // flush((uint64_t*)log_start);
    flush_barrier();
  }

  // printf("nbWrites = %li\n", nbWrites);

  delete [] ptr_l;
  delete [] ptr_l_end;
  return nbReps;
}

