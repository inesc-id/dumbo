/*
 * File:
 *   log.h
 * Author(s):
 *   Ricardo Vieira
 * Description:
 *   STM transaction log.
 *
 */

#ifndef _LOG_H_
#define _LOG_H_

#include "utils.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
// #include <x86intrin.h>

/* ################################################################### *
 * Defines
 * ################################################################### */

#ifndef LOG_TYPE
 #define LOG_TYPE        0      /* Define log type; 0 is explicit, 1 is bitmap, 2 is big log*/
#endif
 #define LOG_AUTO        1      /* Enable or disable automatic logging */
 #define LOG_SIZE        65535     /* Initial log size */
//Use 64*1024 for explicit log, 65535 for bitmap

 #define INT_SIZE         8*sizeof(int)

 //#define PLOCKED
#ifdef PLOCKED
  #define    MALLOC(a)    pmalloc(a)
#else
  #define    MALLOC(a)    xmalloc(a)
#endif

/* ################################################################### *
 * TYPES
 * ################################################################### */

#if LOG_TYPE==0
  #if LOG_AUTO==1
  typedef long* log_struct;
  #else
  typedef int log_struct;
  #endif
#elif LOG_TYPE==1
  typedef uint64_t log_struct;
#elif LOG_TYPE==2
  typedef struct log_struct_t {
    long * pos;        /*Adress written to*/
    long val;        /*Value Written*/
    long time_stamp;  /*Version counter*/
  } log_struct;
#endif

typedef struct tx_logu {
  //int * array;                              /* Array used as log */
  log_struct array[LOG_SIZE];
  int size;                     /* Current log size*/
  int auxPos;                       /* Current log auxPosition*/
  int nb_el;                /* Number of entries in the list */
  struct tx_logu * next;
} tx_logu_t;

typedef struct tx_log {
  tx_logu_t * base;
  tx_logu_t * end;
  tx_logu_t * read;
  long * log_base_pointer;
  int shift_count;
  int nb_el;
} tx_log_t;

/* ################################################################### *
 * INLINE FUNCTIONS
 * ################################################################### */

/* Iinit log */
static INLINE int
stm_log_init(tx_log_t *p) {
  tx_logu_t * t =(tx_logu_t*) MALLOC(sizeof(tx_logu_t));

#if LOG_TYPE!=2
  memset(&t->array, 0, LOG_SIZE*sizeof(log_struct));
#endif
  t->size = LOG_SIZE;
  t->auxPos = 0;
  t->next = NULL;
  p->end = p->base = t;
  p->read = NULL;
  p->log_base_pointer = NULL;
  p->nb_el=1;

  return 0;
};

/*BM INIT*/
static INLINE int
stm_log_initBM(tx_log_t * b, long * point, int size) {
  int i, current;
  b->log_base_pointer = point;

  if (size < LOG_SIZE*64) {
    b->shift_count = 0;
  } else {
    for(i = 1; i<=4; i++) {
      current = size >> i;
      if( current < LOG_SIZE*64) {
        b->shift_count = i;
        break;
      }
    }
  }

  return 1;
}

/* Add value to log*/
static INLINE int
stm_log_newentry(tx_log_t * b, long * pos, int val, long vers ) {
  tx_logu_t * t;

  t = b->end;

#if LOG_TYPE == 0

  if(t->auxPos < t->size) {
#if LOG_AUTO==1
    t->array[t->auxPos] = pos;
#else
    t->array[t->auxPos] = val;
#endif
    t->auxPos = t->auxPos+1;

    return 1;
  } else {
    b->end = t->next = (tx_logu_t*) MALLOC(sizeof(tx_logu_t));
    t = b->end;
    b->nb_el++;
    memset(&t->array, 0, LOG_SIZE*sizeof(log_struct));

#if LOG_AUTO==1
    t->array[0] =  pos;
#else
    t->array[0] =  val;
#endif
    t->auxPos = 1;
    t->size = LOG_SIZE;
    t->next = NULL;

    return 1;
  }

#elif LOG_TYPE == 1
  long index = 0;
#if LOG_AUTO==1
  index = ((long)(void*)(pos - b->log_base_pointer)) ;
#else
  index = val;
#endif
  if ( b->shift_count > 0)
    index = index >> b->shift_count;


  uint64_t array_pos = index >> 6;
  uint64_t mask = index & 63;
  uint64_t write = 1 << mask;
  uint64_t old = t->array[array_pos];

  t->array[array_pos] = old | write;
  t->auxPos = ( array_pos > t->auxPos ) ? array_pos + 1 : t->auxPos;

  return 0;

#elif LOG_TYPE == 2
  if(t->auxPos < t->size) {
    t->array[t->auxPos].pos =  pos;
    t->array[t->auxPos].val =  val;
    t->array[t->auxPos].time_stamp =  vers;

    t->auxPos = t->auxPos+1;

    return 1;
  } else {
    b->end = t->next = (tx_logu_t*) MALLOC(sizeof(tx_logu_t));
    t = b -> end;
    b->nb_el++;

    t->array[0].pos =  pos;
    t->array[0].val =  val;
    t-> auxPos = 1;
    t-> size = LOG_SIZE;
    t-> next = NULL;

    return 1;
  }

#endif
  return 0;
}

/* Read values */
static INLINE tx_logu_t *
stm_log_read(tx_log_t * t ) {
  tx_logu_t * new_t = (tx_logu_t*) MALLOC(sizeof(tx_logu_t));
#if LOG_TYPE!=2
  memset(&new_t->array, 0, LOG_SIZE*sizeof(log_struct));
#endif


  t->read = t->base;
  t->read->nb_el = t->nb_el;

  new_t->size = LOG_SIZE;
  new_t->auxPos = 0;
  new_t->next = NULL;
  t->end = t->base = new_t;
  t->nb_el = 1;

  return t->read;
}

/* Remove an element*/
static INLINE void
stm_logel_free(tx_logu_t * p) {
#ifdef PLOCKED
  free(p);
#else
  free(p);
#endif
}

/* Clean up after yourself*/
static INLINE void
stm_log_free(tx_log_t * t ) {
  tx_logu_t *aux, *aux2;

  aux = aux2 = t->base;

  while(aux!=NULL) {
  aux2 = aux2->next;
    stm_logel_free(aux);

  aux = aux2;
  }

  t->base = t->end = t->read = NULL;

  return;
}


#endif
