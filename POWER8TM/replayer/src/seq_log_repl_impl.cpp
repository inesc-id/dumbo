#include "replayer.hpp"
#include "replayer_internal.hpp"
#include <cstdlib>
#include <cassert>
#include <cstring>

typedef enum {
  SEQL_F_EMPTY = 0,
  SEQL_F_STARTED = 1,
  SEQL_F_FLUSHED = 2,
  SEQL_F_COMMITTED = 4,
  SEQL_F_ABORTED = 8
} seql_flags;

// simulates the sequential log in PM
struct seql_node_t
{
  union {
    uint64_t addr;
    struct {
      uint16_t flags; // seql_flags
      uint16_t tid;
      uint32_t pos_in_log;
    };
  } start_addr;

  uint64_t end_addr;

} __attribute__((packed)); // sizeof(seql_node_t) == 16

static seql_node_t * seq_log_metadata;
static rep::log_entry_t * thread_logs;
static uint64_t * heap;
static rep::args_t g_args;

int rep::seq_log::init(rep::args_t &a)
{
  assert(sizeof(seql_node_t) == 2*sizeof(uint64_t));
  EASY_MALLOC(seq_log_metadata, a.size_metadata);
  memset(seq_log_metadata, 0, a.size_metadata * sizeof(seql_node_t));
  EASY_MALLOC(thread_logs, a.size_thread_log * a.nb_threads);
  EASY_MALLOC(heap, a.heap_size);
  g_args = a;
  return 0;
}

int rep::seq_log::destroy()
{
  free(thread_logs);
  free(seq_log_metadata);
  free(heap);
  return 0;
}

int rep::seq_log::generate_log()
{
  auto ptr_m = seq_log_metadata;
  auto ptr_m_end = seq_log_metadata + g_args.size_metadata;
  rep::log_entry_t** ptr_l = new rep::log_entry_t*[g_args.nb_threads];
  rep::log_entry_t** ptr_l_end = new rep::log_entry_t*[g_args.nb_threads];
  int nbWrTxs = 0;

  for (int i = 0; i < g_args.nb_threads; ++i)
  {
    ptr_l[i] = thread_logs + i * g_args.size_thread_log;
    ptr_l_end[i] = thread_logs + (i+1) * g_args.size_thread_log;
  }

  while (ptr_m < ptr_m_end)
  {
    int t = rep::random(0, g_args.nb_threads-1);
    int w = rep::random(g_args.min_writes, g_args.max_writes);

    if (ptr_l[t] + w >= ptr_l_end[t]) // Not enough space for writes
      break;

    ptr_m->start_addr.tid = t;
    ptr_m->start_addr.flags = SEQL_F_COMMITTED;
    ptr_m->start_addr.pos_in_log = ptr_l[t] - thread_logs;
    ptr_m->end_addr = ptr_l[t] - thread_logs + w;

    while (w-- > 0)
    {
      long p = rep::random_access();
      ptr_l[t]->addr = (uint64_t) (heap + p);
      ptr_l[t]->val = p;
      ptr_l[t]++;
    }

    ptr_m++;
    nbWrTxs++;
  }

  delete [] ptr_l;
  delete [] ptr_l_end;
  return nbWrTxs;
}

int rep::seq_log::replay()
{
  auto ptr_m = seq_log_metadata;
  auto ptr_m_end = seq_log_metadata + g_args.size_metadata;
  int nbReps = 0;
  // long nbWrites = 0; // DEBUG
  
  while (ptr_m < ptr_m_end && ptr_m->start_addr.flags != SEQL_F_EMPTY)
  {    
    // start position in the thread write log
    auto pos_s = thread_logs + ptr_m->start_addr.pos_in_log;

    // end position in the thread write log
    auto pos_e = thread_logs + ptr_m->end_addr;

    // write back the logged values
    while (pos_s < pos_e)
    {
      // write to PM the logged value
      *((uint64_t*)pos_s->addr) = pos_s->val;
      flush((uint64_t*)pos_s->addr);

      // advance the iterator to the thread write log (currently does not wrap-arounds)
      pos_s++;
      // nbWrites++;
    }

    // advance the iterator to the sequential log (currently does not wrap-arounds)
    ptr_m++;
    nbReps++;
    // emulates flush of metadata that flags the workers that there is more log space
    // flush((uint64_t*)pos_s);
    flush_barrier();

    // TODO: the replayer may want to truncate log space to unblock workers doing write transactions
  }

  // printf("nbWrites = %li\n", nbWrites);

  return nbReps;
}

