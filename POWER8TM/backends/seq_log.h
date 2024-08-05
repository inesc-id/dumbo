#ifndef SEQ_LOG_H_GUARD_
#define SEQ_LOG_H_GUARD_

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

// needs to be a power 2 value for bitwise modulo
#define SEQL_MAX_SIZE 0x10000
#define SEQL_MASK     0xFFFF

typedef enum {
  SEQL_F_STARTED = 1,
  SEQL_F_FLUSHED = 2,
  SEQL_F_COMMITTED = 4,
  SEQL_F_ABORTED = 8
} seql_flags;

// simulates the sequential log in PM
typedef struct
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

} __attribute__((packed)) seql_node_t; // sizeof(seql_node_t) == 16
extern seql_node_t *seql_global_ptr; // variable defined in extra_threadc.h

// call this em TM_STARTUP (main thread)
static void seql_init()
{
  assert(sizeof(seql_node_t) == 2*sizeof(uint64_t));
  seql_global_ptr = (seql_node_t*)malloc(sizeof(seql_node_t) * SEQL_MAX_SIZE);
}

// call this em TM_SHUTDOWN (main thread)
static void seql_destroy()
{
  free(seql_global_ptr);
}

#define SEQL_COUNTER_TO_IDX(_counter) \
((_counter) & SEQL_MASK) \
// end of COUNTER_TO_IDX

#define SEQL_START(_c, _tid, _start_ptr) \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr.flags = SEQL_F_STARTED; \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr.tid = _tid; \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr.addr = _start_ptr; \
  /* __atomic_thread_fence(__ATOMIC_RELEASE); */ /* TODO: I think it is not needed */ \
// end of SEQL_BEGIN

#define SEQL_COMMIT(_c, _end_ptr) \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr.flags = SEQL_F_COMMITTED; \
  /* *** flush+fence *** */ \
  __dcbst(&(seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr), 0); /* TODO: sometimes it breaks the ROTs */ \
  emulate_pm_slowdown(); \
  /* *** *********** *** */ \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].end_addr = _end_ptr; \
  /* TODO: flush+flush */ \
  __dcbst(&(seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].end_addr), 0); /* TODO: sometimes it breaks the ROTs */ \
  emulate_pm_slowdown(); \
  /* *** *********** *** */ \
// end of SEQL_COMMIT
// TODO: we now assume that the background replayer is fast enough to clear the log (more or less the same as infinite log)

#define SEQL_ABORT(_c) \
  seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr.flags = SEQL_F_ABORTED; \
  /* *** flush+fence *** */ \
  __dcbst(&(seql_global_ptr[SEQL_COUNTER_TO_IDX(_c)].start_addr), 0); /* TODO: sometimes it breaks the ROTs */ \
  //emulate_pm_slowdown(); \
  /* *** *********** *** */ \
// end of SEQL_COMMIT

#endif /* SEQ_LOG_H_GUARD_ */
