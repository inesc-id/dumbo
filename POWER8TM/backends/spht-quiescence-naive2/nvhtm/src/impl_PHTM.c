#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "htm_impl.h"

static volatile __thread PHTM_log_s *phtm_log = NULL;
/* extern */volatile PHTM_marker_pool_s *phtm_markers; // shared
/* extern */volatile PHTM_log_s **phtm_logs;
static volatile __thread PHTM_marker_pool_s *phtm_markers_local;

static volatile __thread uint64_t readClockVal;

static volatile __thread
  int writeLogStart, writeLogEnd;

static volatile __thread uint64_t timeWaitingTS1 = 0;
static volatile __thread uint64_t timeWaitingTS2 = 0;
static volatile __thread uint64_t timeWaiting = 0;

static volatile __thread uint64_t countCommitPhases = 0;

static volatile uint64_t incCommitsPhases = 0;
static volatile uint64_t incTimeTotal = 0;
static volatile uint64_t incAfterTx = 0;
static volatile uint64_t incWaiting = 0;

enum LOG_CODES_ {
	NOT_MARKED = -2, MARKED = -1
};

#define GET_CL_ADDR(addr) ({ \
	intptr_t cl_addr; \
	intptr_t cl_bits_off = -1; \
	cl_bits_off = cl_bits_off << 6; \
	cl_addr = (intptr_t) addr; \
	cl_addr = cl_addr & cl_bits_off; \
	cl_addr; \
})\

#define PHTM_log_size(tid) ({ phtm_log->size; })

#define MARKER_POINTER(pool, addr) ({ \
	((intptr_t)addr >> 6) & (pool->nb_markers - 1); \
})

static int PHTM_mark_addr(void* addr, int tid, int log_rec);
static inline int PHTM_log_cache_line(int tid, void* addr, uint64_t val);
static void PHTM_rem_mark(void* addr);
static void PHTM_log_clear();

void install_bindings_PHTM()
{
  on_before_htm_begin  = on_before_htm_begin_PHTM;
  on_htm_abort         = on_htm_abort_PHTM;
  on_before_htm_write  = on_before_htm_write_8B_PHTM;
  on_before_htm_commit = on_before_htm_commit_PHTM;
  on_after_htm_commit  = on_after_htm_commit_PHTM;

  wait_commit_fn = wait_commit_PHTM;
}

void state_gather_profiling_info_PHTM(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
}

void state_fprintf_profiling_info_PHTM(char *filename)
{
  FILE *fp = fopen(filename, "a+");
  if (fp == NULL) {
    printf("Cannot open %s! Try to remove it.\n", filename);
    return;
  }
  fseek(fp, 0L, SEEK_END);
  if ( ftell(fp) < 8 ) {
      fprintf(fp, "#%s\t%s\t%s\t%s\t%s\n",
              "NB_THREADS",
              "NB_COMMIT_PHASES",
              "TIME_TOTAL",
              "TIME_AFTER_TX",
              "TIME_WAIT");
  }
  fprintf(fp, "%i\t%lu\t%lu\t%lu\t%lu\n", gs_appInfo->info.nbThreads,
    incCommitsPhases, incTimeTotal, incAfterTx, incWaiting);
}

void on_before_htm_begin_PHTM(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  MEASURE_TS(timeTotalTS1);
  // TODO: this goes in some init callback
  if (phtm_log == NULL) {
    // use the log space from the other solutions
    phtm_log = phtm_logs[threadId];
    phtm_markers_local = phtm_markers;
  }
}

void on_htm_abort_PHTM(int threadId) { /* empty */ }

void on_before_htm_write_8B_PHTM(int threadId, void *addr, uint64_t val)
{ 
  PHTM_log_cache_line(threadId, addr, val);

  // if (HTM_test()) { // in HTM write-through
  //   PHTM_log_cache_line(threadId, addr, val);
  // } else { // outside HTM need an undo log
  //   // TODO: postpone the write
  //   PHTM_log_cache_line(threadId, addr, val);
	// }
}

void on_before_htm_commit_PHTM(int threadId)
{
  /* TODO: emulate transparent flush and special commit */

  phtm_log->is_persistent = 1;
}

void on_after_htm_commit_PHTM(int threadId)
{
  MEASURE_TS(timeTotalTS2);
  MEASURE_INC(countCommitPhases);
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeTotalTS2, timeTotal);

  int nb_writes = PHTM_log_size(threadId);
  if (nb_writes) {
    // -----------------------------------
    // TODO: this must go inside the HTM
    FLUSH_RANGE(&phtm_log->records[0], &phtm_log->records[phtm_log->size],
      &phtm_log->records[0], &phtm_log->records[PHTM_LOG_MAX_REC]);
    FLUSH_RANGE(&phtm_log->addrs[0], &phtm_log->addrs[phtm_log->size],
      &phtm_log->addrs[0], &phtm_log->addrs[PHTM_LOG_MAX_REC]);
    FLUSH_CL(&(phtm_log->is_persistent));
    FENCE_PREV_FLUSHES();
    // -----------------------------------
	int i;
    for (i = 0; i < nb_writes; ++i) {
			if (phtm_log->addrs[i] != 0) {
				// TODO: bug after 16 threads....
      	FLUSH_CL(phtm_log->addrs[i]); // flushes the actual memory before releasing the logs
			}
    }
    FENCE_PREV_FLUSHES();
  }
	PHTM_log_clear();
}

void wait_commit_PHTM(int threadId) { /* empty */ }

// --------------

static inline void cpy_to_cl(int rec_idx, intptr_t *addr);

// NO LONGER USED, NV-HTM log is repurposed 
// static PHTM_log_s *PHTM_create_log(int tid)
// {
// 	PHTM_log_s *res;
// 	printf("created log for %i\n", tid);
// 	// TODO: check, res is NULL
// 	// ALLOC_FN(res, PHTM_log_s, sizeof (PHTM_log_s));
//   res = (PHTM_log_s*) &(P_write_log[tid][0]);
// 	// res = (PHTM_log_s*) malloc(sizeof (PHTM_log_s));
// 	// memset(res, 0, sizeof (PHTM_log_s));
// 	res->size = 0;
// 	res->is_persistent = 0;
// 	res->records[0].cache_line[0] = 0;
// 	res->records[1].cache_line[0] = 0;
// 	res->records[2].cache_line[0] = 0;
// 	res->records[3].cache_line[0] = 0;
// 	return res;
// }

static int PHTM_mark_addr(void* addr, int tid, int log_rec)
{
	intptr_t ptr = MARKER_POINTER(phtm_markers_local, addr);
	PHTM_marker_s *marker = &phtm_markers_local->markers[ptr];

	// marked before, return address
	if (marker->tid == tid) {
		return marker->ptr_to_log;
	}
	// marked by other TX
	if (marker->tid != -1) {
		return MARKED;
	}

	marker->ptr_to_log = log_rec;
	marker->tid = tid;

	// still not in use
	return NOT_MARKED;
}

static void PHTM_rem_mark(void* addr)
{
	intptr_t ptr = MARKER_POINTER(phtm_markers_local, addr);
	PHTM_marker_s *marker = &phtm_markers_local->markers[ptr];

	__atomic_store_n(&(marker->tid), -1, __ATOMIC_RELEASE);
}

static inline int PHTM_log_cache_line(int tid, void* addr, uint64_t val)
{
	size_t size = phtm_log->size;
	int rec_idx = size;
	int in_marker = PHTM_mark_addr(addr, tid, rec_idx);
	intptr_t cl_addr;
	int i;

	if (size >= PHTM_LOG_MAX_REC) {
		// write limit exceeded
    // TODO: needs some warning
		return 1;
	}

	if (in_marker == MARKED) {
		// abort
		if (HTM_test()) { // cache line is locked by other transaction
			HTM_abort();
		}
		// The SGL waits the others
		while ((in_marker = PHTM_mark_addr(addr, tid, rec_idx)) == -1);
	}

	cl_addr = GET_CL_ADDR(addr);

	if (in_marker == NOT_MARKED) {
		// new entry

		phtm_log->addrs[rec_idx] = cl_addr;
		cpy_to_cl(rec_idx, (intptr_t*) cl_addr);
		phtm_log->size += 1;
		return 1;
	} else {
		// same TX accessed the cache line
		rec_idx = in_marker;

		if (phtm_log->addrs[rec_idx] != cl_addr) {
			// aliasing in the locks ---> must search a new entry
			for (i = rec_idx + 1; i < size; ++i) {
				if (phtm_log->addrs[i] == cl_addr) {
					// found
					cpy_to_cl(i, (intptr_t*) cl_addr);
					phtm_log->addrs[i] = cl_addr;
					break;
				}
			}

			if (i == size) {
				// not found, add in the end
				cpy_to_cl(i, (intptr_t*) cl_addr);
				phtm_log->addrs[rec_idx] = cl_addr;
				// phtm_log->size += 1;
				// int buf = phtm_log->size + 1; // at least updating the pointer is PM
				phtm_log->size += 1;
				// MN_write(&(phtm_log->size), &buf, sizeof(int), 0);
			}
		}
	}

	return 1;
}

/* int PHTM_log_size(int tid) {
return phtm_log->size;
} */

static void PHTM_log_clear()
{
	int i;

	for (i = 0; i < phtm_log->size; ++i) {
		PHTM_rem_mark((void*) phtm_log->addrs[i]);
	}

	phtm_log->is_persistent = 0;
	phtm_log->size = 0;
  FLUSH_CL(&(phtm_log->is_persistent));
  FENCE_PREV_FLUSHES();
	// phtm_log->size = 0;
	// phtm_log->is_persistent = 0;
	// __sync_synchronize();
}

static inline void cpy_to_cl(int rec_idx, intptr_t *addr) {
  intptr_t* cl_addr = (intptr_t*)GET_CL_ADDR(addr);

	// TODO: only add intptr_t if cache line is present
	// copy cache line to phtm_log
	volatile int i;
	for (i = 0; i < ARCH_CACHE_LINE_SIZE / sizeof(int64_t); ++i) { //  / sizeof(int64_t)
		((int64_t*)phtm_log->records[rec_idx].cache_line)[i] = ((int64_t*)cl_addr)[i];
		// phtm_log->records[rec_idx].cache_line[i] = cl_addr[i];
	}

	// memcpy is bad in HTM --> syscall?
}
