/* =============================================================================
 *
 * pisces.c
 *
 * Implementation of the Pisces STM algorithm, according to the pseudo-code
 * presented in the paper:
 * "Pisces: A Scalable and Efficient Persistent Transactional Memory" (ATC'19)
 *
 * =============================================================================
 *
 * Contributors: João Pedro Barreto (joao.barreto@tecnico.ulisboa.pt)
 *
 * =============================================================================
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <signal.h>
#include "platform.h"
#include "util.h"


// __INLINE__ long ReadSetCoherent (Thread*);


# define delay_for_pm 70
# define emulate_pm_slowdown() \
{ \
  volatile int i; \
  for (i = 0; i < delay_for_pm; ++i) \
    __asm__ volatile ("nop" ::: "memory"); \
}


#ifndef READ_TIMESTAMP
# define READ_TIMESTAMP(dest) __asm__ volatile("0: \n\tmfspr   %0,268 \n": "=r"(dest));
#endif 


enum pisces_config {
    pisces_INIT_WRSET_NUM_ENTRY = 1024,
    // pisces_INIT_RDSET_NUM_ENTRY = 8192,
    pisces_INIT_LOCAL_NUM_ENTRY = 1024,
};

// typedef int            BitMap;




/* Write-set log entry */
typedef struct _AVPair {
    struct _AVPair* Next;
    struct _AVPair* Prev;
    volatile intptr_t* Addr;
    intptr_t Valu;
    long Ordinal;
} AVPair;

typedef struct _Log {
    AVPair* List;
    AVPair* put;        /* Insert position - cursor */
    AVPair* tail;       /* CCM: Pointer to last valid entry */
    AVPair* end;        /* CCM: Pointer to last entry */
    long ovf;           /* Overflow - request to grow */
    long persistTS;
    // BitMap BloomFilter; /* Address exclusion fast-path test */
} Log;

typedef struct _lock
{
    // Thread *writer;
    AVPair *avp;
    struct _lock *next;
} lock_t;


struct _Thread {
    long UniqID;

    volatile long Retries;
    long Starts;
    long Aborts; /* Tally of # of aborts */
    long flush_time;
    long upd_dur_commit_time;
    long ro_wait_time;
    long isolation_wait_time;
    long ro_tx_time;
    long upd_tx_time;
    long abort_time;
    long upd_commits;
    long ro_commits;
    
    long tsBegin_for_stats;

    int numStores;
    int numLoads;

    // long snapshot;
    // unsigned long long rng;
    // unsigned long long xorrng [1];
    // Log rdSet;
    Log wrSet;
    // volatile unsigned long cm_backoff;               
    // volatile unsigned long cm_seed;  
    sigjmp_buf* envPtr;

    // Pisces-specific
    volatile int isActive;
    volatile int inCritical;
    volatile unsigned long startTS;
    volatile unsigned long endTS;
};


/* Based on TL2's lock table code */

#define _TABSZ (1 << 20)
#define TABMSK (_TABSZ - 1)
#define COLOR (128)
#define PSSHIFT ((sizeof(void *) == 4) ? 2 : 3)

static unsigned long start_time, end_time;

static volatile lock_t *locked_avpairs[_TABSZ];
static volatile Thread *lock_owners[_TABSZ];

void assert_empty_locks(Thread *self) {
    for (int i = 0; i < _TABSZ; i++) {
        assert(lock_owners[i]!=self);
        // if (lock_owners[i]==NULL)
        //     assert(locked_avpairs[i] == NULL);
    }
}




//map variable address to lock address.
//TODO: revert to the following commented lines
#define TAB_OFFSET(a) ((UNS(a) & TABMSK))
//define TAB_OFFSET(a) ((((UNS(a)+COLOR) >> PSSHIFT) & TABMSK)) /* PS1M */

#define LOCKED_AVPAIRS(a) (locked_avpairs + TAB_OFFSET(addr))
#define LOCK_OWNER(a) (lock_owners + TAB_OFFSET(addr))


#define MAX_THREADS 64
static volatile Thread* threads[MAX_THREADS];




typedef struct
{
    long value;
    char suffixPadding[128];
} aligned_type_t ;

__attribute__((aligned(128))) volatile aligned_type_t* LOCK;


static pthread_key_t    global_key_self;

// void TxIncClock() {
//     LOCK->value = LOCK->value + 2;
// }

// long  TxReadClock() {
//     return LOCK->value;
// }

#ifndef pisces_CACHE_LINE_SIZE
#  define pisces_CACHE_LINE_SIZE           (64)
#endif

// __INLINE__ unsigned long long
// MarsagliaXORV (unsigned long long x)
// {
//     if (x == 0) {
//         x = 1;
//     }
//     x ^= x << 6;
//     x ^= x >> 21;
//     x ^= x << 7;
//     return x;
// }

// __INLINE__ unsigned long long
// MarsagliaXOR (unsigned long long* seed)
// {
//     unsigned long long x = MarsagliaXORV(*seed);
//     *seed = x;
//     return x;
// }

// __INLINE__ unsigned long long
// TSRandom (Thread* Self)
// {
//     return MarsagliaXOR(&Self->rng);
// }

__INLINE__ intptr_t
AtomicAdd (volatile intptr_t* addr, intptr_t dx)
{
    intptr_t v;
    for (v = *addr; CAS(addr, v, v+dx) != v; v = *addr) {}
    return (v+dx);
}

volatile long StartTally         = 0;
volatile long AbortTally         = 0;

volatile long flush_timeTally = 0;
volatile long upd_dur_commit_timeTally  = 0;
volatile long ro_wait_timeTally = 0;
volatile long isolation_wait_timeTally = 0;
volatile long ro_tx_timeTally = 0;
volatile long upd_tx_timeTally = 0;
volatile long abort_timeTally = 0;
volatile long stm_commitsTally = 0;

volatile long stm_ro_commitsTally = 0;
volatile long stm_upd_commitsTally = 0;


volatile long ReadOverflowTally  = 0;
volatile long WriteOverflowTally = 0;
volatile long LocalOverflowTally = 0;
#define pisces_TALLY_MAX          (((unsigned long)(-1)) >> 1)

// #define FILTERHASH(a)                   ((UNS(a) >> 2) ^ (UNS(a) >> 5))
// #define FILTERBITS(a)                   (1 << (FILTERHASH(a) & 0x1F))

__INLINE__ AVPair*
MakeList (long sz, Thread* Self)
{
    AVPair* ap = (AVPair*) malloc((sizeof(*ap) * sz) + pisces_CACHE_LINE_SIZE);
    assert(ap);
    memset(ap, 0, sizeof(*ap) * sz);
    AVPair* List = ap;
    AVPair* Tail = NULL;
    long i;
    for (i = 0; i < sz; i++) {
        AVPair* e = ap++;
        e->Next    = ap;
        e->Prev    = Tail;
        e->Ordinal = i;
        Tail = e;
    }
    Tail->Next = NULL;

    return List;
}

 void FreeList (Log*, long) __attribute__ ((noinline));
/*__INLINE__*/ void
FreeList (Log* k, long sz)
{
    /* Free appended overflow entries first */
    AVPair* e = k->end;
    if (e != NULL) {
        while (e->Ordinal >= sz) {
            AVPair* tmp = e;
            e = e->Prev;
            free(tmp);
        }
    }

    /* Free continguous beginning */
    free(k->List);
}

__INLINE__ AVPair*
ExtendList (AVPair* tail)
{
    AVPair* e = (AVPair*)malloc(sizeof(*e));
    assert(e);
    memset(e, 0, sizeof(*e));
    tail->Next = e;
    e->Prev    = tail;
    e->Next    = NULL;
    e->Ordinal = tail->Ordinal + 1;
    return e;
}

// __INLINE__ void
// WriteBackForward (Log* k)
// {
//     AVPair* e;
//     AVPair* End = k->put;
//     for (e = k->List; e != End; e = e->Next) {
//         *(e->Addr) = e->Valu;
//     }
// }

void
TxOnce ()
{
    LOCK = (aligned_type_t*) malloc(sizeof(aligned_type_t));
    LOCK->value = 0;

    pthread_key_create(&global_key_self, NULL); /* CCM: do before we register handler */

READ_TIMESTAMP(start_time);
}


void
TxShutdown ()
{
    printf("pisces system shutdown:\n"
           "  Starts=%li Aborts=%li\n"
           "  Overflows: R=%li W=%li L=%li\n"
           , StartTally, AbortTally,
           ReadOverflowTally, WriteOverflowTally, LocalOverflowTally);

READ_TIMESTAMP(end_time);

    // printf("pisces system shutdown:\n"
    //        "  Starts=%li Aborts=%li\n"
    //        "  Overflows: R=%li W=%li L=%li\n"
    //        , StartTally, AbortTally,
    //        ReadOverflowTally, WriteOverflowTally, LocalOverflowTally);

  unsigned long total_time = end_time - start_time; \

  

// printf(\
// "Total sum time: %lu\n" \
// "Total commit time: %lu\n" \
// "Total abort time: %lu\n" \
// "Total wait time: %lu\n" \
// "Total sus time: %lu\n" \
// "Total flush time: %lu\n" \
// "Total dur_commit time: %lu\n" \
// "Total RO_dur_wait time: %lu\n" \
// "Total upd tx time: %lu\n" \
// "Total RO tx time: %lu\n" \
// "Total commits: %lu\n" \
//   "\tRead commits: %lu\n" \
//   "\tHTM commits:  %lu\n" \
//   "\tROT commits:  %lu\n" \
//   "\tGL commits: %lu\n" \
// "Total aborts: %lu\n" \
//   "\tHTM conflict aborts:  %lu\n" \
//     "\t\tHTM self aborts:  %lu\n" \
//     "\t\tHTM trans aborts:  %lu\n" \
//     "\t\tHTM non-trans aborts:  %lu\n" \
//   "\tHTM user aborts :  %lu\n" \
//   "\tHTM capacity aborts:  %lu\n" \
//     "\t\tHTM persistent aborts:  %lu\n" \
//   "\tHTM other aborts:  %lu\n" \
//   "\tROT conflict aborts:  %lu\n" \
//     "\t\tROT self aborts:  %lu\n" \
//     "\t\tROT trans aborts:  %lu\n" \
//     "\t\tROT non-trans aborts:  %lu\n" \
//     "\t\tROT other conflict aborts:  %lu\n" \
//   "\tROT user aborts:  %lu\n" \
//   "\tROT capacity aborts:  %lu\n" \
//     "\t\tROT persistent aborts:  %lu\n" \
//   "\tROT other aborts:  %lu\n", \


  printf(\
"Total sum time: %lu\n" \
"Total commit time: %lu\n" \
"Total abort-upd time: %lu\n" \
"Total abort-ro time: %lu\n" \
"Total wait time: %lu\n" \
"Total sus time: %lu\n" \
"Total flush time: %lu\n" \
"Total dur_commit time: %lu\n" \
"Total RO_dur_wait time: %lu\n" \
"Total upd tx time: %lu\n" \
"Total RO tx time: %lu\n" \
"Total commits: %lu\n" \
  "\tNon-tx commits: %lu\n" \
  "\tHTM commits:  %lu\n" \
  "\tROT commits:  %lu\n" \
  "\tGL commits: %lu\n" \
  "\tSTM commits:  %lu\n" \
  "\t\tSTM RO commits:  %lu\n" \
  "\t\tSTM upd commits:  %lu\n" \
"Total aborts: %lu\n" \
  "\tHTM conflict aborts:  %lu\n" \
    "\t\tHTM self aborts:  %lu\n" \
    "\t\tHTM trans aborts:  %lu\n" \
    "\t\tHTM non-trans aborts:  %lu\n" \
  "\tHTM user aborts :  %lu\n" \
  "\tHTM capacity aborts:  %lu\n" \
    "\t\tHTM persistent aborts:  %lu\n" \
  "\tHTM other aborts:  %lu\n" \
  "\tROT conflict aborts:  %lu\n" \
    "\t\tROT self aborts:  %lu\n" \
    "\t\tROT trans aborts:  %lu\n" \
    "\t\tROT non-trans aborts:  %lu\n" \
    "\t\tROT other conflict aborts:  %lu\n" \
  "\tROT user aborts:  %lu\n" \
  "\tROT capacity aborts:  %lu\n" \
    "\t\tROT persistent aborts:  %lu\n" \
  "\tROT other aborts:  %lu\n", \
  0,
  0,
  abort_timeTally,
  0, //in pisces, RO never abort
  isolation_wait_timeTally,
  0,
  flush_timeTally,
  upd_dur_commit_timeTally,
  ro_wait_timeTally,
  upd_tx_timeTally,
  ro_tx_timeTally,
  stm_commitsTally, 
  0,
  0, 
  0,
  0,
  stm_commitsTally,
  stm_ro_commitsTally,
  stm_upd_commitsTally,
  AbortTally, 
  AbortTally,
  0,
  AbortTally,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  0);

  printf("Emulated PM latency: %d\n", delay_for_pm);

    pthread_key_delete(global_key_self);
    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i])
            threads[i] = NULL;
    }

    MEMBARSTLD();
}


Thread*
TxNewThread ()
{
    Thread* t = (Thread*)calloc(sizeof(Thread),1);
    assert(t);

    

    return t;
}


void
TxFreeThread (Thread* t)
{
    // AtomicAdd((volatile intptr_t*)((void*)(&ReadOverflowTally)),  t->rdSet.ovf);

    long wrSetOvf = 0;
    Log* wr;
    wr = &t->wrSet;
    {
        wrSetOvf += wr->ovf;
    }
    AtomicAdd((volatile intptr_t*)((void*)(&WriteOverflowTally)), wrSetOvf);

    AtomicAdd((volatile intptr_t*)((void*)(&StartTally)),         t->Starts);
    AtomicAdd((volatile intptr_t*)((void*)(&AbortTally)),         t->Aborts);


    AtomicAdd((volatile intptr_t*)((void*)(&flush_timeTally)),         t->flush_time);
    AtomicAdd((volatile intptr_t*)((void*)(&upd_dur_commit_timeTally)),         t->upd_dur_commit_time);
    AtomicAdd((volatile intptr_t*)((void*)(&ro_wait_timeTally)),         t->ro_wait_time);
    AtomicAdd((volatile intptr_t*)((void*)(&isolation_wait_timeTally)),         t->isolation_wait_time);
    AtomicAdd((volatile intptr_t*)((void*)(&ro_tx_timeTally)),         t->ro_tx_time);
    AtomicAdd((volatile intptr_t*)((void*)(&upd_tx_timeTally)),         t->upd_tx_time);
    AtomicAdd((volatile intptr_t*)((void*)(&abort_timeTally)),         t->abort_time);

    AtomicAdd((volatile intptr_t*)((void*)(&stm_commitsTally)),         t->upd_commits);
    AtomicAdd((volatile intptr_t*)((void*)(&stm_upd_commitsTally)),         t->upd_commits);
    AtomicAdd((volatile intptr_t*)((void*)(&stm_commitsTally)),         t->ro_commits);
    AtomicAdd((volatile intptr_t*)((void*)(&stm_ro_commitsTally)),         t->ro_commits);

    AtomicAdd((volatile intptr_t*)((void*)(&AbortTally)),         t->Aborts);



    // FreeList(&(t->rdSet),     pisces_INIT_RDSET_NUM_ENTRY);
    // FreeList(&(t->wrSet),     pisces_INIT_WRSET_NUM_ENTRY);

    // free(t);
}

void
TxInitThread (Thread* t, long id)
{
    /* CCM: so we can access pisces's thread metadata in signal handlers */
    pthread_setspecific(global_key_self, (void*)t);

    memset(t, 0, sizeof(*t));     /* Default value for most members */

    t->UniqID = id;
    // t->rng = id + 1;
    // t->xorrng[0] = t->rng;

    t->wrSet.List = MakeList(pisces_INIT_WRSET_NUM_ENTRY, t);
    t->wrSet.put = t->wrSet.List;

    threads[id] = t;

    // t->rdSet.List = MakeList(pisces_INIT_RDSET_NUM_ENTRY, t);
    // t->rdSet.put = t->rdSet.List;

    // t->cm_seed = 123456789UL;
    // t->cm_backoff=4;
}

__INLINE__ void
txReset (Thread* Self)
{
    //TO DO GC of previous AVPAirs (which now are simply mem leaked)
    // Self->wrSet.List = MakeList(pisces_INIT_WRSET_NUM_ENTRY, Self);

    Self->wrSet.put = Self->wrSet.List;
    Self->wrSet.tail = NULL;

    /* Pisces specific */
    Self->isActive = 0;
    Self->inCritical = 0;
    MEMBARLDLD();

    Self->numLoads = 0;
    Self->numStores  = 0;

    READ_TIMESTAMP(Self->tsBegin_for_stats);
    //TODO: free locks and AVpairs, careful with gc rules of pisces

}

__INLINE__ void
txCommitReset (Thread* Self)
{
    txReset(Self);
    Self->Retries = 0;
    // Self->cm_backoff=4;
}

// // returns -1 if not coherent
// __INLINE__ long
// ReadSetCoherent (Thread* Self)
// {
//     long time;
//     while (1) {
//         time = LOCK->value;
// 	MEMBARSTLD();
//         if ((time & 1) != 0) {
//             continue;
//         }

//         Log* const rd = &Self->rdSet;
//         AVPair* const EndOfList = rd->put;
//         AVPair* e;

//         for (e = rd->List; e != EndOfList; e = e->Next) {
//             if (e->Valu != LDNF(e->Addr)) {
//                 return -1;
//             }
//         }

//         if (LOCK->value == time)
//             break;
//     }
//     return time;
// }


// __INLINE__ void
// backoff (Thread* Self, long attempt)
// {
//     unsigned long long stall = TSRandom(Self) & 0xF;
//     stall += attempt >> 2;
//     stall *= 10;
//     /* CCM: timer function may misbehave */
//     volatile typeof(stall) i = 0;
//     while (i++ < stall) {
//         PAUSE();
//     }
// }


static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int result=0;
  unsigned long int upper, lower,tmp;
  __asm__ volatile(
                "0:                  \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b         \n"
                : "=r"(upper),"=r"(lower),"=r"(tmp)
                );
  result = upper;
  result = result<<32;
  result = result|lower;

  return(result);
}

// __INLINE__ long
// TryFastUpdate (Thread* Self)
// {
//     Log* const wr = &Self->wrSet;
//     long ctr;
//     //unsigned long long int begin = rdtsc();    
//     while (CAS(&(LOCK->value), Self->snapshot, Self->snapshot + 1) != Self->snapshot) {
//         long newSnap = ReadSetCoherent(Self);
//         if (newSnap == -1) {
//             return 0; //TxAbort(Self);
//         }
//         Self->snapshot = newSnap;
//     }

//     {
//         WriteBackForward(wr); /* write-back the deferred stores */
//     }
//     MEMBARSTST(); /* Ensure the above stores are visible  */
//     LOCK->value = Self->snapshot + 2;
//     MEMBARSTLD();
//     //unsigned long long int length = rdtsc() - begin;
//     //printf("length of commit phase: %lu\n",length);
//     return 1; /* success */
// }

void
TxAbort (Thread* Self)
{
  
//    printf("Aborted\n");

  // unsigned long wait;
  // volatile int j;
  // Self->cm_seed ^= (Self->cm_seed << 17);
  // Self->cm_seed ^= (Self->cm_seed >> 13);
  // Self->cm_seed ^= (Self->cm_seed << 5);
  // wait = Self->cm_seed % Self->cm_backoff;
  // //printf("MAX_BACKOFF is %ld, wait is %ld, seed is %ld and backoff is %ld\n",MAX_BACKOFF,wait,Self->cm_seed,Self->cm_backoff);
  // for (j = 0; j < wait; j++);
  // if (Self->cm_backoff < MAX_BACKOFF)
  //    Self->cm_backoff <<= 1;

  // Releases all locks held by this tx
    AVPair *e;
    AVPair *End = Self->wrSet.put;
    // printf("Thread %d is aborting, removing locks in entries:", Self->UniqID);
    for (e = Self->wrSet.List; e != End; e = e->Next)
    {
        remove_lock(e->Addr, Self);
        // printf("%x, ", TAB_OFFSET(e->Addr));
        // printf("TxAbort: removed lock from %x\n", e->Addr);
    }
    // printf("\n");

    MEMBARSTLD();
    
    unsigned long tsAbort;
    READ_TIMESTAMP(tsAbort);
    Self->abort_time += tsAbort - Self->tsBegin_for_stats;

    txReset(Self);
    Self->Retries++;
    Self->Aborts++;

    
    // usleep(rand() % 1000);

    // assert_empty_locks(Self);

    SIGLONGJMP(*Self->envPtr, 1);
    ASSERT(0);
}
//pthread_mutex_t cas_mutex;


lock_t *get_locked_avpair(volatile intptr_t* addr) {
    lock_t *l = *(LOCKED_AVPAIRS(addr));

    while (l) {
        if (l->avp->Addr == addr)
            return l;
        l = l->next;
    }
    return NULL;
}

int remove_lock(volatile intptr_t* addr, Thread *self) {

    Thread *owner = *(LOCK_OWNER(addr));

    lock_t **l_a = LOCKED_AVPAIRS(addr);
    // assert(!owner || *l_a);

    if (*l_a) {
        *l_a = NULL;
        MEMBARLDLD();
        *(LOCK_OWNER(addr)) = NULL;
    }

    return 1;

    // while (*l_a) {
    //     lock_t *l_b = (*l_a);
    //     if (l_b->avp->Addr == addr)
    //     {
    //         if (CAS(l_a, l_b, l_b->next) == l_b)
    //             //TODO gc (lock entry and AVPair)
    //             return 1;
    //         else
    //             continue;
    //     }
    //     else
    //         l_a = &(l_b->next);
    // }
    
    //TO DO: free avpairs in list  

}

void assert_locked_wrset(Thread *self) {
    AVPair *End = self->wrSet.put;
    for (AVPair *e = self->wrSet.List; e != End; e = e->Next)
    {
        // assert(*(LOCK_OWNER(e->Addr)) == self);
        assert((get_locked_avpair(e->Addr)->avp) == e);
    }
}




void
TxStore (Thread* Self, volatile intptr_t* addr, intptr_t valu)
{
    Self->numStores ++;

    //lock_t *prev_lock = *(PSLOCK(addr));
    // assert_locked_wrset(Self);
    Thread *owner = *LOCK_OWNER(addr);

    if (owner && owner != Self) {
        // printf("Aborted: thread %d, addr=%p, owner=%d\n", Self->UniqID, addr, owner->UniqID);
        TxAbort(Self);
        assert(0); //I should never reach this point
        return;
    }

    if (owner == Self) {
        lock_t *l = get_locked_avpair(addr);
        if (l) {
            l->avp->Valu = valu;
            assert(owner == *LOCK_OWNER(addr));
            // assert_locked_wrset(Self);
            return;
        }
    }
    else {
        if (CAS(LOCK_OWNER(addr), NULL, Self) != NULL) {
            // printf("I will abort because CAS on txstore failed\n");
            TxAbort(Self);
            assert(0); //I should never reachi this point
            return;
        }
        lock_t *l = get_locked_avpair(addr);
        assert(l == NULL);
        // assert(*LOCKED_AVPAIRS(addr) == NULL);
    }

    // assert_locked_wrset(Self);
    assert (*LOCK_OWNER(addr) == Self);


    Log* k = &Self->wrSet;
    AVPair* e = k->put;
    if (e == NULL) {
    	k->ovf++;
    	e = ExtendList(k->tail);
    	k->end = e;
    }
    k->tail    = e;
    k->put     = e->Next;
    e->Addr    = addr;
    e->Valu    = valu;

    lock_t * new_lock = (lock_t *)malloc(sizeof(lock_t));
    new_lock->avp = e;

    // new_lock->writer = Self;

    lock_t *prev_lock = *(LOCKED_AVPAIRS(addr));
    new_lock->next = prev_lock;
    MEMBARLDLD();

    *(LOCKED_AVPAIRS(addr)) = new_lock;

    // if (CAS(PSLOCK(addr), prev_lock, new_lock) != prev_lock)
    //     TxAbort(Self);

    assert (TxLoad(Self, addr) == valu);

    assert (*LOCK_OWNER(addr) == Self);

    // assert_locked_wrset(Self);

    return;
}

intptr_t
TxLoad (Thread* Self, volatile intptr_t* addr)
{
    Self->numLoads ++;
    // assert_locked_wrset(Self);
    

    lock_t *l = get_locked_avpair(addr);

    if (l == NULL)
        return LDNF(addr);

    Thread *wtx = *(LOCK_OWNER(addr));
    if (wtx == Self) {
        // assert_locked_wrset(Self);
        return l->avp->Valu;
    }
    
    unsigned long t1, t2;
    READ_TIMESTAMP(t1);
    while (wtx && wtx->inCritical) {/* wait */
        // printf("Thread %d waiting in txLoad\n", Self->UniqID);
    }
    READ_TIMESTAMP(t2);
    Self->ro_wait_time += t2-t1;


    unsigned long endTS_lido;
   
   if (wtx) endTS_lido = wtx->endTS;

    if (wtx && endTS_lido <= Self->startTS) {
    // if (wtx && wtx->endTS <= Self->startTS) {
        // printf("\n\n LEU DE LOCK DE OUTRO OWNER \n");
        // assert_locked_wrset(Self);
        return l->avp->Valu;
    }
    else {
        // assert_locked_wrset(Self);
        return LDNF(addr);
    }
}


void
TxStart (Thread* Self, sigjmp_buf* envPtr)
{
    txReset(Self);

    // printf("TxStart (Self %x)\n", Self);

    /* Pisces specific */
    Self->isActive = 1;
    Self->startTS = LOCK->value;
    Self->endTS = -1;

    Self->numLoads = 0;
    Self->numStores  = 0;

    MEMBARLDLD();

    Self->envPtr= envPtr;
    Self->Starts++;
}

// assertNoLocks() {
//     for (int i=0; i<_TABSZ; i++) {
//         assert(lock_tab[i] == 0);
//     }
//     printf("assertNoLocks OK\n");
// }

int
TxCommit (Thread* Self)
{

    printf("Loads: %d, \tStores: %d \n", Self->numLoads, Self->numStores);
        
    unsigned long tBeforeCommit;
    READ_TIMESTAMP(tBeforeCommit);

    Self->isActive = 0;

    //If log is empty, it's a read-only tx
    if (Self->wrSet.put == Self->wrSet.List)
    {
        Self->ro_tx_time += tBeforeCommit - Self->tsBegin_for_stats;
        txCommitReset(Self);
        // printf("*********************** committed tx with 0 writes (RO)\n");
        
        //debug - apagar
        // assert_empty_locks(Self);
        Self->ro_commits ++;
        
        return 1;
    }

    Self->upd_tx_time += tBeforeCommit - Self->tsBegin_for_stats;
    
    /* stage 1: persist stage */

    //*Emulate* flush wr log
    AVPair* e;
    AVPair* End = Self->wrSet.put;
    for (e = Self->wrSet.List; e != End; e = e->Next) {
        emulate_pm_slowdown();
    }

    Self->wrSet.persistTS = LOCK->value;
    //*Emulate* flush persistTS
    emulate_pm_slowdown();

    /* stage 2: concurrency commit stage */

    Self->inCritical = 1;

    MEMBARLDLD();

    Self->endTS = LOCK->value + 1;
    Self->inCritical = 0;
    AtomicAdd(&(LOCK->value), 1);

    unsigned long tBeforeWriteBack;
    READ_TIMESTAMP(tBeforeWriteBack);

    Self->upd_dur_commit_time += tBeforeWriteBack - tBeforeCommit;

    /* stage 3: write-back stage */

    unsigned long tBeforeWait;
    READ_TIMESTAMP(tBeforeWait);

    for (int i = 0; i < MAX_THREADS; i++) {
        if (threads[i]) {
            while (threads[i]->isActive && threads[i]->startTS < Self->endTS)
                {/* wait */
                    // printf("Thread %d waiting in txCommit\n", Self->UniqID);
                }
        }
    }

    unsigned long tAfterWait;
    READ_TIMESTAMP(tAfterWait);
    Self->isolation_wait_time += tAfterWait-tBeforeWait;

    // printf("TxCommit: Self %x. Writeset: ", Self);

    End = Self->wrSet.put;
    int aux = 0;
    for (e = Self->wrSet.List; e != End; e = e->Next)
    {
        *(e->Addr) = e->Valu;
        //*Emulate* pflush(copy.source.content)
        emulate_pm_slowdown();
        MEMBARLDLD();
        remove_lock(e->Addr, Self);
        // printf("%x, ", e->Addr);
        aux++;
    }
    // printf("\n");

    unsigned long tAfterWriteBack;
    READ_TIMESTAMP(tAfterWriteBack);
    Self->flush_time += tAfterWriteBack-tAfterWait;

    // printf("*********************** committed tx with %d writes\n", aux);
    txCommitReset(Self);

    //debug - apagar
    // assert_empty_locks(Self);

    Self->upd_commits ++;

    return 1;

    // TxAbort(Self);
    // return 0;
}

int
TxCommitSTM (Thread* Self)
{
    return TxCommit(Self);
}

// long TxValidate (Thread* Self) {
//     if (Self->wrSet.put == Self->wrSet.List) {
//         return -1;
//     } else {
//         long local_global_clock = LOCK->value;

//         while (1) {
//             Log* const rd = &Self->rdSet;
//             AVPair* const EndOfList = rd->put;
//             AVPair* e;

//             for (e = rd->List; e != EndOfList; e = e->Next) {
//                 if (e->Valu != LDNF(e->Addr)) {
//                     TxAbort(Self);
//                 }
//             }

//             long tmp = LOCK->value;
//             if (local_global_clock == tmp) {
//                 return local_global_clock;
//             } else {
//                 local_global_clock = tmp;
//             }
//         }
//         return local_global_clock;
//     }
// }


// long TxFinalize (Thread* Self, long clock) {
//     if (Self->wrSet.put == Self->wrSet.List) {
//         txCommitReset(Self);
//         return 0;
//     }

//     if (LOCK->value != clock) {
//         return 1;
//     }

//     Log* const wr = &Self->wrSet;
//     WriteBackForward(wr); /* write-back the deferred stores */
//     LOCK->value += LOCK->value + 2;

//     return 0;
// }

// void TxResetAfterFinalize (Thread* Self) {
//     txCommitReset(Self);
// }
