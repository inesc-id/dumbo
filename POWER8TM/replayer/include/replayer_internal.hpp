#pragma once
#include "replayer.hpp"
#include <cstdint>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifndef EASY_MALLOC
#define ARCH_CACHE_LINE_SIZE 128
#define MALLOC_FN(size) aligned_alloc(ARCH_CACHE_LINE_SIZE, (size))
#define EASY_MALLOC(var, nb) \
  if ((var = (__typeof__(var))MALLOC_FN(sizeof(__typeof__(*(var)))*(nb))) == NULL) \
    fprintf(stderr, "malloc(%zu): %s\n", sizeof(__typeof__(*(var)))*(nb), strerror(errno));
#endif /* EASY_MALLOC */

#define bit63one              0x8000000000000000lu
#define bit62one              0x4000000000000000lu
#define isbit63one(ts)        ((ts & bit63one)==bit63one)
#define isbit62one(ts)        ((ts & bit62one)==bit62one)
#define flipBit63(ts)         ( bit63one ^ (ts))
#define onesBit63(ts)         ( bit63one | (ts))
#define zeroBit63(ts)         (~bit63one & (ts))
#define store_fence()         __atomic_thread_fence(__ATOMIC_RELEASE)
#define load_fence()          __atomic_thread_fence(__ATOMIC_ACQUIRE)
#define atomic_STORE(ptr,val) __atomic_store_n(&(ptr), val, __ATOMIC_RELEASE)
#define atomic_LOAD(ptr)      __atomic_load_n(&(ptr), __ATOMIC_ACQUIRE)

#define __dcbst(base, index)  \
  __asm__ ("dcbst %0, %1" : /*no result*/ : "b%" (index), "r" (base) : "memory")

// - n=10 (43ns, preprint do Izrael*)
// - n=46 (eurosys'22)
// - n=66 (TPP -> extrapolation to PM/CXL)
extern int pm_delay /* = 46 */;
extern thread_local int nb_cache_lines;

#define flush(_cache_line_addr) ({ \
  nb_cache_lines ++; \
  __dcbst(_cache_line_addr, 0); \
})

#define flush_barrier() ({\
  volatile int i, j; \
  for (i = 0; i < nb_cache_lines; ++i) \
    for (j = 0; j < pm_delay; ++j) \
      { __asm__ volatile ("nop" ::: "memory"); } \
  __asm__ volatile ("sync" ::: "memory"); \
  nb_cache_lines = 0; \
})

namespace rep
{

struct log_entry_t
{
  union {
    struct {
      uint64_t addr;
      uint64_t val;
    };
    struct {
      uint64_t is_commit; // bit in position 63 to one
      uint64_t ts; // TODO: not used
    };
    struct {
      uint64_t commit_ts; // bit in position 63 to one
      log_entry_t* link_next;
    };
  };
} __attribute__((packed));

namespace naive
{

int init(args_t&);
int generate_log();
int replay();
int destroy();

}

namespace forward_link
{

int init(args_t&);
int generate_log();
int replay();
int destroy();

}

namespace seq_log
{

int init(args_t&);
int generate_log();
int replay();
int destroy();

}

}
