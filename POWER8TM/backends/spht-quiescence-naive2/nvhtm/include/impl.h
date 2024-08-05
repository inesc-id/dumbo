#ifndef IMPL_H_GUARD
#define IMPL_H_GUARD

#include "global_structs.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <setjmp.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Joao: this is duplicated code from elsewhere */
#define rmb()           asm volatile ("sync" ::: "memory")
#define rwmb()           asm volatile ("lwsync" ::: "memory")
#define cpu_relax()     asm volatile ("" ::: "memory");


// #ifdef NPROFILE
// #define MEASURE_TS(ts_var) /* empty */
// #define MEASURE_INC(counter) /* empty */
// #define INC_PERFORMANCE_COUNTER(ts_var1, ts_var2, counter) /* empty */
// #else
#define MEASURE_TS(ts_var) ts_var = rdtscp()
#define MEASURE_INC(counter) counter++
#define INC_PERFORMANCE_COUNTER(ts_var1, ts_var2, counter) counter += ts_var2 - ts_var1
// #endif

// These are always done
#define A_MEASURE_TS(ts_var) ts_var = rdtscp()
#define A_MEASURE_INC(counter) counter++
#define A_INC_PERFORMANCE_COUNTER(ts_var1, ts_var2, counter) counter += ts_var2 - ts_var1

#ifdef USE_BITWISE_FOR_MOD2
#define MOD_FN(_arg, _mod) ((_arg) & (_mod-1))
#else
#define MOD_FN(_arg, _mod) ((_arg) % (_mod))
#endif

extern int isCraftySet;
extern volatile __thread int crafty_isValidate;
extern __thread long nbTransactions;

#define NV_HTM_BEGIN(_threadId, ro) \
  nbTransactions++;\
  crafty_isValidate = 0; \
  while (1) { /* TODO: this needs to be in the same function (else screws the stack) */ \
    jmp_buf env; \
    on_before_htm_begin(_threadId, (int) ro); \
    if (ro) {\
      RO_begin(); \
    }\
    else {\
      HTM_SGL_begin(_threadId); \
    } \
    MEASURE_TS(timeTotalTS1); \
//

#define NV_HTM_END(_threadId) \
  MEASURE_TS(timeAfterTXTS1); \
  if (ro) { \
    RO_commit();\
  } \
  else {\
    /*onBeforeHtmCommit(_threadId); */\
    HTM_SGL_commit(); \
  } \
  /*if (!ro) on_after_htm_commit(_threadId); */\
  MEASURE_TS(timeTotalTS2); \
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeTotalTS2, timeTotal); \
  INC_PERFORMANCE_COUNTER(timeAfterTXTS1, timeTotalTS2, timeAfterTXSuc); \
  extern volatile __thread uint64_t successCommitPhase; \
  successCommitPhase++; \
  break; \
} \
//



// -----------------------------------------
// PHTM
#define PHTM_NB_MARKERS  1048576
#define PHTM_LOG_MAX_REC 512

typedef struct PHTM_marker_
{
	int tid; // use -1 to mark as empty
	int ptr_to_log;
} CL_ALIGN /*__attribute__((packed))*/ PHTM_marker_s;

typedef struct PHTM_marker_pool_
{
	PHTM_marker_s *markers;
	size_t nb_markers;
} CL_ALIGN PHTM_marker_pool_s;

// log

typedef struct phtm_log_rec_
{
	volatile int64_t cache_line[ARCH_CACHE_LINE_SIZE / sizeof(int64_t)]; // CACHE_LINE_SIZE
} __attribute__((packed)) PHTM_log_rec_s;

typedef struct phtm_log_
{
	PHTM_log_rec_s records[PHTM_LOG_MAX_REC];
	intptr_t addrs[PHTM_LOG_MAX_REC];
	int is_persistent;
	size_t size; /* size is pointer to next entry */
} CL_ALIGN PHTM_log_s;


extern volatile PHTM_log_s **phtm_logs;
extern volatile PHTM_marker_pool_s *phtm_markers;
// -----------------------------------------

// -----------------------------------------
// cc-HTM
#define CCHTM_QUEUE_SIZE 1048576

#define CC_HTM_RED_Q_EMPTY ((uint64_t)-1)
#define CC_HTM_RED_Q_HEAD  ((uint64_t)-2)
#define CC_HTM_RED_Q_END   ((uint64_t)-3)
#define CC_HTM_RED_Q_BUSY  ((uint64_t)-4)

typedef struct cc_htm_queue_entry_
{
	uint64_t startTS;
	uint64_t endTS;
	uint64_t logPtr;
	uint64_t persistTS;
	uint64_t nextRedIdx;
} CL_ALIGN /* __attribute__((packed)) */ cc_htm_queue_entry_s;

typedef struct cc_htm_queue_
{
  uint64_t txCounter;
  uint64_t txMin;
  uint64_t redHeadIdx;

  cc_htm_queue_entry_s entries[CCHTM_QUEUE_SIZE];
} cc_htm_queue_s;

extern volatile cc_htm_queue_s *ccHTM_Q;
// -----------------------------------------

void learn_spin_nops(
  double latencyInNanoSec,
  double cpuFreqInKiloHertz,
  int isForceTest
);

// initialize with this to change the implementation to use
void install_bindings_pc();
void install_bindings_htmOnly();
void install_bindings_ccHTM();
void install_bindings_PHTM();
void install_bindings_pcwc();
void install_bindings_pcwc2();
void install_bindings_pcwm();
void init_stats_pcwm();
void install_bindings_pcwm2();
void install_bindings_pcwm3();
void install_bindings_crafty();
void install_bindings_lc();
void install_bindings_ub();
void install_bindings_epoch_sync();
void install_bindings_epoch_impa();
void install_bindings_epoch_static_deadline();

/* TESTS ******************************************************************** */
void state_log_checker_pcwc(void(*waitFn)(), int isAfterRun);

void state_gather_profiling_info_epoch_sync(int threadId);
void state_gather_profiling_info_epoch_impa(int threadId);
void state_gather_profiling_info_lc(int threadId);
//void state_gather_profiling_info_pc(int threadId);
void state_gather_profiling_info_htmOnly(int threadId);
void state_gather_profiling_info_ccHTM(int threadId);
void state_gather_profiling_info_PHTM(int threadId);
void state_gather_profiling_info_ub(int threadId);
void state_gather_profiling_info_pcwc(int threadId);
void state_gather_profiling_info_pcwc2(int threadId);
void state_gather_profiling_info_pcwm(int threadId);
void state_gather_profiling_info_pcwm2(int threadId);
void state_gather_profiling_info_pcwm3(int threadId);
void state_gather_profiling_info_crafty(int threadId);

void state_fprintf_profiling_info_epoch_sync(char *filename);
void state_fprintf_profiling_info_epoch_impa(char *filename);
void state_fprintf_profiling_info_lc(char *filename);
void state_fprintf_profiling_info_pc(char *filename);
void state_fprintf_profiling_info_htmOnly(char *filename);
void state_fprintf_profiling_info_ccHTM(char *filename);
void state_fprintf_profiling_info_PHTM(char *filename);
void state_fprintf_profiling_info_ub(char *filename);
void state_fprintf_profiling_info_pcwc(char *filename);
void state_fprintf_profiling_info_pcwc2(char *filename);
void state_fprintf_profiling_info_pcwm(char *filename);
void state_fprintf_profiling_info_pcwm2(char *filename);
void state_fprintf_profiling_info_pcwm3(char *filename);
void state_fprintf_profiling_info_crafty(char *filename);
/* ************************************************************************** */

typedef enum {
  LOG_REPLAY_FORWARD  = 0b0001,
  LOG_REPLAY_BACKWARD = 0b0010,
  LOG_REPLAY_ASYNC_SORTER = 0b0100,
  LOG_REPLAY_LOGICAL_CLOCKS  = 0b00010000,
  LOG_REPLAY_PHYSICAL_CLOCKS = 0b00100000, // TODO: PCWM make use of the marker
  LOG_REPLAY_BUFFER_FLUSHES  = 0b01000000, // buffers in set then flush all cache lines
  LOG_REPLAY_BUFFER_WBINVD   = 0b10000000, // use the WBINVD instruction in the end (avoid buffer set)
  LOG_REPLAY_PHYSICAL_CLOCKS_SORTED = 0b100000000, // PCWM2
  LOG_REPLAY_RANGE_FLUSHES = 0b1000000000, //
  LOG_REPLAY_CC_HTM = 0b10000000000, //
  LOG_REPLAY_CC_HTM_NO_W = 0b100000000000, //
  LOG_REPLAY_PARALLEL = 0b1000000000000, //
  LOG_REPLAY_PHYSICAL_CLOCKS_SORTED_BACKWARD = 0b10000000000000, // PCWM3
  LOG_REPLAY_CONCURRENT = 0b100000000000000 // concurrent apply
} LOG_REPLAY_FLAGS; 

extern LOG_REPLAY_FLAGS log_replay_flags;
extern char log_replay_stats_file[];
extern char malloc_stats_file[];

void replay_log_init(
  int _nbThreads,
  int _nbReplayers,
  uint64_t _logSize,
  volatile cache_line_s *_logPtrs,
  volatile uint64_t **_writeLog,
  volatile uint64_t *_latestSafeTsv/* PCWM */,
  volatile cc_htm_queue_s *_ccHTM_Q/* PCWM */,
  LOG_REPLAY_FLAGS flags,
  void **_nvramRanges,
  int _nbNvramRanges
);
void replay_log_destroy();
uint64_t replay_log_total_bytes();
void replay_log_print_stats(char *filename);

void replay_log_apply(); // run all

// crafty needs this to be set...
//void crafty_set_respawn_point(jmp_buf *env);

//void on_before_htm_begin_pc(int threadId);
//void on_before_htm_begin_pcwc(int threadId);
//void on_before_htm_begin_pcwc2(int threadId);
void on_before_htm_begin_pcwm(int threadId, int ro);
void on_before_htm_begin_pcwm2(int threadId, int ro);
//void on_before_htm_begin_pcwm3(int threadId);
void on_before_htm_begin_crafty(int threadId);
void on_before_htm_begin_ccHTM(int threadId);
void on_before_htm_begin_PHTM(int threadId);
void on_before_htm_begin_htmOnly(int threadId);
//void on_before_htm_begin_lc(int threadId);
//void on_before_htm_begin_ub(int threadId);
//void on_before_htm_begin_epoch_sync(int threadId);
void on_before_htm_begin_epoch_impa(int threadId);
//void on_before_htm_begin_epoch_static_deadline(int threadId);

//void on_htm_abort_pc(int threadId);
void on_htm_abort_htmOnly(int threadId);
void on_htm_abort_ccHTM(int threadId);
void on_htm_abort_PHTM(int threadId);
//void on_htm_abort_pcwc(int threadId);
//void on_htm_abort_pcwc2(int threadId);
void on_htm_abort_pcwm(int threadId);
void on_htm_abort_pcwm2(int threadId);
//void on_htm_abort_pcwm3(int threadId);
void on_htm_abort_crafty(int threadId);
//void on_htm_abort_lc(int threadId);
//void on_htm_abort_ub(int threadId);
//void on_htm_abort_epoch_sync(int threadId);
void on_htm_abort_epoch_impa(int threadId);
//void on_htm_abort_epoch_static_deadline(int threadId);

//void on_before_htm_write_8B_pc(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_htmOnly(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_ccHTM(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_PHTM(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_pcwc(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_pcwc2(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_pcwm(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_pcwm2(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_pcwm3(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_crafty(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_lc(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_ub(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_epoch_sync(int threadId, void *addr, uint64_t val);
void on_before_htm_write_8B_epoch_impa(int threadId, void *addr, uint64_t val);
//void on_before_htm_write_8B_epoch_static_deadline(int threadId, void *addr, uint64_t val);

//void on_before_htm_commit_pc(int threadId);
void on_before_htm_commit_htmOnly(int threadId);
void on_before_htm_commit_ccHTM(int threadId);
void on_before_htm_commit_PHTM(int threadId);
//void on_before_htm_commit_pcwc(int threadId);
//void on_before_htm_commit_pcwc2(int threadId);
void on_before_htm_commit_pcwm(int threadId);
void on_before_htm_commit_pcwm2(int threadId);
//void on_before_htm_commit_pcwm3(int threadId);
void on_before_htm_commit_crafty(int threadId);
//void on_before_htm_commit_lc(int threadId);
//void on_before_htm_commit_ub(int threadId);
//void on_before_htm_commit_epoch_sync(int threadId);
void on_before_htm_commit_epoch_impa(int threadId);
//void on_before_htm_commit_epoch_static_deadline(int threadId);

//void on_after_htm_commit_pc(int threadId);
void on_after_htm_commit_htmOnly(int threadId);
void on_after_htm_commit_ccHTM(int threadId);
void on_after_htm_commit_PHTM(int threadId);
//void on_after_htm_commit_pcwc(int threadId);
//void on_after_htm_commit_pcwc2(int threadId);
void on_after_htm_commit_pcwm(int threadId);
void on_after_htm_commit_pcwm2(int threadId);
//void on_after_htm_commit_pcwm3(int threadId);
void on_after_htm_commit_crafty(int threadId);
//void on_after_htm_commit_lc(int threadId);
//void on_after_htm_commit_ub(int threadId);
//void on_after_htm_commit_epoch_sync(int threadId);
void on_after_htm_commit_epoch_impa(int threadId);
//void on_after_htm_commit_epoch_static_deadline(int threadId);

void try_prune_log_epoch_impa(int threadId);
void prune_log_forward_epoch_impa(int threadId); /* called by try_prune_log_impa */

void try_prune_log_epoch_sync(int threadId);
void prune_log_forward_epoch_sync(int threadId);

void try_prune_log_epoch_static_deadline(int threadId);
void prune_log_forward_epoch_static_deadline(int threadId);

//void wait_commit_pc_simple(int threadId);
void wait_commit_htmOnly(int threadId);
void wait_commit_ccHTM(int threadId);
void wait_commit_PHTM(int threadId);
//void wait_commit_pc_bitmap(int threadId);
//void wait_commit_pcwc(int threadId);
//void wait_commit_pcwc2(int threadId);
void wait_commit_pcwm(int threadId);
void wait_commit_pcwm2(int threadId);
//void wait_commit_pcwm3(int threadId);
void wait_commit_crafty(int threadId);
//void wait_commit_lc(int threadId);
//void wait_commit_ub(int threadId);
//void wait_commit_epoch_sync(int threadId);
void wait_commit_epoch_impa(int threadId);
//void wait_commit_epoch_static_deadline(int threadId);

//void *craftyMalloc(int tid, long size);

// -----------------------------------------------------------------------------

#ifdef NDEBUG
#define DEBUGPRINT2(...)               /* empty */
#define DEBUGPRINT(_fmt, G ...)        /* empty */
#define DEBUG_ASSERT(_cond, _fmt, ...) /* empty */
#else /* DEBUG */
#define WHERESTR  "[%s:%d]: "
#define WHEREARG  __FILE__, __LINE__
#define DEBUGPRINT2(...)       fprintf(stderr, __VA_ARGS__)
#define DEBUGPRINT(_fmt, ...)  DEBUGPRINT2(WHERESTR _fmt, WHEREARG, __VA_ARGS__)
#define DEBUG_ASSERT(_cond, _fmt, ...) \
  if (!(_cond)) { DEBUGPRINT(_fmt, __VA_ARGS__); exit(EXIT_FAILURE); }
#endif /* NDEBUG */

// -----------------------------------------------------------------------------
// steal bit
#define flipBit63(_64b_uint) (( (uint64_t)1 << 63) ^ (_64b_uint))
#define onesBit63(_64b_uint) (( (uint64_t)1 << 63) | (_64b_uint))
#define zeroBit63(_64b_uint) (((uint64_t)-1 >>  1) & (_64b_uint))
#define isBit63One(_64b_uint) ((((uint64_t)1 << 63) & (_64b_uint)) >> 63)
// is flushed bit
#define flipBit62(_64b_uint) (( (uint64_t)1 << 62) ^ (_64b_uint))
#define onesBit62(_64b_uint) (( (uint64_t)1 << 62) | (_64b_uint))
#define zeroBit62(_64b_uint) ((((uint64_t)-1 >> 2) | (uint64_t)1 << 63)) & (_64b_uint))
#define isBit62One(_64b_uint) ((((uint64_t)1 << 62) & (_64b_uint)) >> 62)

#define zeroBit62and63(_64b_uint) (((uint64_t)-1 >> 2) & (_64b_uint))

#define EPOCH_PTR(_tid) (G_next[_tid].padded_ptr.ptr)
#define EPOCH_LAST(threadID) __atomic_load_n(&(EPOCH_PTR(threadID)), __ATOMIC_ACQUIRE)
#define EPOCH_PREVIOUS(threadID) \
  (EPOCH_PTR(threadID) == 0 ? 0 : EPOCH_PTR(threadID) - 1)
#define EPOCH_MINUS2(threadID) \
  (EPOCH_PTR(threadID) < 2 ? EPOCH_PTR(threadID) : EPOCH_PTR(threadID) - 2)
#define EPOCH_EMPTY_PREVIOUS(threadID) \
  (P_epoch_ts[threadID][(MOD_FN(EPOCH_PREVIOUS(threadID), gs_appInfo->info.allocEpochs)] = 0)

#define EPOCH_READ_PTR(ptr, threadID) \
  __atomic_load_n(&(P_epoch_ts[threadID][MOD_FN((ptr), gs_appInfo->info.allocEpochs)]), __ATOMIC_ACQUIRE)
#define EPOCH_READ_PTR_U(ptr, threadID) /* NOTE: the _U does not do modulo */\
  __atomic_load_n(&(P_epoch_ts[threadID][ptr].ts), __ATOMIC_ACQUIRE)
#define EPOCH_READ(threadID) \
  __atomic_load_n(&(P_epoch_ts[threadID][MOD_FN(EPOCH_LAST(threadID), gs_appInfo->info.allocEpochs)]), __ATOMIC_ACQUIRE)
#define EPOCH_READ_NEXT_N(threadID, _n) \
  __atomic_load_n(&(P_epoch_ts[threadID][MOD_FN(((EPOCH_LAST(threadID)+_n), gs_appInfo->info.allocEpochs)]), __ATOMIC_ACQUIRE)
#define EPOCH_READ_BEFORE_PREVIOUS(threadID) \
  __atomic_load_n(&(P_epoch_ts[threadID][MOD_FN(EPOCH_MINUS2(threadID), gs_appInfo->info.allocEpochs)]), __ATOMIC_ACQUIRE)

/* epoch write must also update the log pointers*/
#define EPOCH_WRITE(threadID, tsReading) \
  __atomic_store_n(&(P_epoch_ts[threadID][EPOCH_LAST(threadID)]), tsReading, __ATOMIC_RELEASE); \
  FLUSH_CL(&(P_epoch_ts[threadID][EPOCH_LAST(threadID)]));
#define EPOCH_WRITE_VAL(threadID, tsReading, _epoch) \
  __atomic_store_n(&(P_epoch_ts[threadID][_epoch]), tsReading, __ATOMIC_RELEASE); \
  FLUSH_CL(&(P_epoch_ts[threadID][_epoch]));
#define EPOCH_FINALIZE(threadID) \
  EPOCH_PTR(threadID) = MOD_FN((EPOCH_PTR(threadID) + 1), gs_appInfo->info.allocEpochs)

// -----------------------------------------------------------------------------
#define IMPATIENT_EPOCH_NEXT_INC(threadID) \
  (EPOCH_PTR(threadID) = MOD_FN((EPOCH_PTR(threadID) + 1), gs_appInfo->info.allocEpochs))
#define IMPATIENT_EPOCH_END_INC(pruned)                                   \
  __atomic_store_n(                                                       \
    &gs_log_data.log.epoch_end,                                           \
    MOD_FN((gs_log_data.log.epoch_end + (pruned)), gs_appInfo->info.allocEpochs), \
    __ATOMIC_RELEASE);

#define IMPATIENT_EPOCH_READ(threadID) EPOCH_READ(threadID)

// attempts to reserve own epoch slot with read TS
#define TRY_CAS_EPOCH_SLOT(_tid, _slot, _readTS, _ts) \
  __sync_bool_compare_and_swap(&(P_epoch_ts[_tid][_slot]), _readTS, _ts)

// if (_epoch == P_start_epoch) use other approach
#define IS_SLOT_OCCUPIED_NON_START(_epoch, _tid) \
    (zeroBit62and63(P_epoch_ts[_tid][_epoch]) > zeroBit62and63(P_epoch_ts[_tid][P_start_epoch]))

// TODO: check with the prev: when _epoch == P_start_epoch it does not work!!!
#define IS_SLOT_OCCUPIED_START(_epoch, _tid) \
  zeroBit62and63(P_epoch_ts[_tid][MOD_FN((_epoch + gs_appInfo->info.allocEpochs - 1), \
  gs_appInfo->info.allocEpochs)]) < zeroBit62and63(P_epoch_ts[_tid][_epoch])

#define IS_SLOT_OCCUPIED(_epoch1, _tid1) ({ \
  int _res = 0; \
  if (_epoch1 == P_start_epoch) { \
    _res = IS_SLOT_OCCUPIED_START(_epoch1, _tid1); \
  } else { \
    _res = IS_SLOT_OCCUPIED_NON_START(_epoch1, _tid1); \
  } \
  _res; \
})

// TS version avoids loading the remote TS multiple times
#define IS_SLOT_OCCUPIED_TS_NON_START(_epoch, _tid, _ts) \
    (_ts > zeroBit62and63(P_epoch_ts[_tid][P_start_epoch]))

#define IS_SLOT_OCCUPIED_TS_START(_epoch, _tid, _ts) \
  (zeroBit62and63(P_epoch_ts[_tid][MOD_FN((_epoch + gs_appInfo->info.allocEpochs - 1), \
  gs_appInfo->info.allocEpochs)]) < _ts)

#define IS_SLOT_OCCUPIED_TS(_epoch1, _tid1, _ts1) ({ \
  int _res = 0; \
  if (_epoch1 == P_start_epoch) { \
    _res = IS_SLOT_OCCUPIED_TS_START(_epoch1, _tid1, _ts1); \
  } else { \
    _res = IS_SLOT_OCCUPIED_TS_NON_START(_epoch1, _tid1, _ts1); \
  } \
  _res; \
})

#define IS_SLOT_UNSYNC(_epoch, _tid, _myTid) \
  (zeroBit62and63(P_epoch_ts[_tid][_epoch]) < zeroBit62and63(P_epoch_ts[_myTid][ \
    (_epoch + gs_appInfo->info.allocEpochs - 1) % gs_appInfo->info.allocEpochs]))

// TODO: log boundaries
#define IS_CLOSE_TO_END(_epochPtr, _endPtr) ( \
  (_epochPtr) == (_endPtr) \
  || MOD_FN(((_epochPtr) + 1), gs_appInfo->info.allocEpochs) == (_endPtr) \
  || MOD_FN(((_epochPtr) + 2), gs_appInfo->info.allocEpochs) == (_endPtr) \
  || MOD_FN(((_epochPtr) + 3), gs_appInfo->info.allocEpochs) == (_endPtr) \
  || MOD_FN(((_epochPtr) + 4), gs_appInfo->info.allocEpochs) == (_endPtr) \
  || MOD_FN(((_epochPtr) + 5), gs_appInfo->info.allocEpochs) == (_endPtr) \
)
//

// does not block
#define LOOK_UP_FREE_SLOT(_tid) ({ \
  int32_t * volatile _slotPtr = (int32_t * volatile)&G_next[_tid].log_ptrs.epoch_next; \
  volatile int32_t _resSlot = *_slotPtr; \
  volatile int32_t _epochEnd = gs_log_data.log.epoch_end; \
  int i;\
  for (i = 0; i < gs_appInfo->info.nbThreads; ++i) { /* finds the maximum */ \
    volatile int32_t _remoteEpoch = G_next[i].log_ptrs.epoch_next; \
    if ((_resSlot > _epochEnd && _remoteEpoch > _epochEnd) || (_resSlot < _epochEnd && _remoteEpoch < _epochEnd)) { \
      if (_resSlot < _epochEnd) \
        _resSlot = MOD_FN(_remoteEpoch + gs_appInfo->info.allocEpochs -1, gs_appInfo->info.allocEpochs); \
    } else if (_resSlot > _epochEnd && _remoteEpoch < _epochEnd) \
      _resSlot = MOD_FN(_remoteEpoch + gs_appInfo->info.allocEpochs -1, gs_appInfo->info.allocEpochs); \
  } \
  while (IS_SLOT_OCCUPIED(_resSlot, _tid) && !gs_appInfo->info.isExit) { \
    if (IS_CLOSE_TO_END(_resSlot, gs_log_data.log.epoch_end) && !gs_appInfo->info.isExit) { \
      break; \
    } \
    _resSlot = MOD_FN((_resSlot + 1), gs_appInfo->info.allocEpochs); \
  } \
  __atomic_store_n(_slotPtr, _resSlot, __ATOMIC_RELEASE); \
  _resSlot; \
})

#define IS_EPOCH_AFTER(_epochBase, _epochToCheck) ({ \
  int _res2 = 0; \
  if ((_epochBase >= gs_log_data.log.epoch_end && _epochToCheck >= gs_log_data.log.epoch_end) \
      || (_epochBase <= gs_log_data.log.epoch_end && _epochToCheck <= gs_log_data.log.epoch_end)) { \
    _res2 = _epochToCheck > _epochBase; \
  } \
  else if (_epochBase > gs_log_data.log.epoch_end && _epochToCheck < gs_log_data.log.epoch_end) { \
    _res2 = 1; /* _tsToCheck is ahead, after the wrap */ \
  } \
  else if (_epochBase <= gs_log_data.log.epoch_end && _epochToCheck >= gs_log_data.log.epoch_end) { \
    _res2 = 0; /* _tsToCheck is behind, before the wrap */ \
  } \
  _res2; \
}) \
//

#define FIND_LAST_SAFE_EPOCH() ({ \
  volatile int32_t _lastEpoch = __atomic_load_n(&G_next[0].log_ptrs.epoch_next, __ATOMIC_ACQUIRE); \
  int _i;\
  for (_i = 1; _i < gs_appInfo->info.nbThreads; ++_i) { \
    if (!IS_EPOCH_AFTER(_lastEpoch, __atomic_load_n(&G_next[_i].log_ptrs.epoch_next, __ATOMIC_ACQUIRE))) { \
      _lastEpoch = G_next[_i].log_ptrs.epoch_next; \
    } \
  } \
  _lastEpoch = MOD_FN((_lastEpoch + gs_appInfo->info.allocEpochs - 1), gs_appInfo->info.allocEpochs); \
  _lastEpoch; \
})

// -----------------------------------------------------------------------------

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* IMPL_H_GUARD */
