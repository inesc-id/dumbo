#include "impl.h"
#include "spins.h"
#include "rdtsc.h"

#include <stdio.h>
#include <limits.h>
#include <stdint.h>

#include "htm_impl.h"

typedef uintptr_t bit_array_t;

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

void install_bindings_htmOnly()
{
  on_before_htm_begin  = on_before_htm_begin_htmOnly;
  on_htm_abort         = on_htm_abort_htmOnly;
  on_before_htm_write  = on_before_htm_write_8B_htmOnly;
  on_before_htm_commit = on_before_htm_commit_htmOnly;
  on_after_htm_commit  = on_after_htm_commit_htmOnly;

  wait_commit_fn = wait_commit_htmOnly;
}

void state_gather_profiling_info_htmOnly(int threadId)
{
  __sync_fetch_and_add(&incCommitsPhases, countCommitPhases);
  __sync_fetch_and_add(&incTimeTotal, timeTotal);
  __sync_fetch_and_add(&incAfterTx, timeAfterTXSuc);
  __sync_fetch_and_add(&incWaiting, timeWaiting);
}

void state_fprintf_profiling_info_htmOnly(char *filename)
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

void on_before_htm_begin_htmOnly(int threadId)
{
  onBeforeWrite = on_before_htm_write;
  onBeforeHtmCommit = on_before_htm_commit;
  MEASURE_TS(timeTotalTS1);
}

void on_htm_abort_htmOnly(int threadId) { /* empty */ }

void on_before_htm_write_8B_htmOnly(int threadId, void *addr, uint64_t val) { /* empty */ }

void on_before_htm_commit_htmOnly(int threadId) { /* empty */ }

void on_after_htm_commit_htmOnly(int threadId)
{
  MEASURE_TS(timeTotalTS2);
  MEASURE_INC(countCommitPhases);
  INC_PERFORMANCE_COUNTER(timeTotalTS1, timeTotalTS2, timeTotal);
}

void wait_commit_htmOnly(int threadId) { /* empty */ }
