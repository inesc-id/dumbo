/* =============================================================================
 *
 * queue.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * For the license of bayes/sort.h and bayes/sort.c, please see the header
 * of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of kmeans, please see kmeans/LICENSE.kmeans
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of ssca2, please see ssca2/COPYRIGHT
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/mt19937ar.c and lib/mt19937ar.h, please see the
 * header of the files.
 * 
 * ------------------------------------------------------------------------
 * 
 * For the license of lib/rbtree.h and lib/rbtree.c, please see
 * lib/LEGALNOTICE.rbtree and lib/LICENSE.rbtree
 * 
 * ------------------------------------------------------------------------
 * 
 * Unless otherwise noted, the following license applies to STAMP files:
 * 
 * Copyright (c) 2007, Stanford University
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 * 
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 * 
 *     * Neither the name of Stanford University nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY STANFORD UNIVERSITY ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * =============================================================================
 */


#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "random.h"
#include "tm.h"
#include "types.h"
#include "queue-stm.h"

using namespace queue_stm;

/* =============================================================================
 * queue_alloc
 * =============================================================================
 */
queue_t*
queue_stm::queue_alloc (long initCapacity)
{
    queue_t* queuePtr = (queue_t*)malloc(sizeof(queue_t));

    if (queuePtr) {
        long capacity = ((initCapacity < 2) ? 2 : initCapacity);
        queuePtr->elements = (void**)malloc(capacity * sizeof(void*));
        if (queuePtr->elements == NULL) {
            free(queuePtr);
            return NULL;
        }
        queuePtr->pop      = capacity - 1;
        queuePtr->push     = 0;
        queuePtr->capacity = capacity;
    }

    return queuePtr;
}


/* =============================================================================
 * Pqueue_alloc
 * =============================================================================
 */
queue_t*
queue_stm::Pqueue_alloc (long initCapacity)
{
    queue_t* queuePtr = (queue_t*)P_MALLOC(sizeof(queue_t));

    if (queuePtr) {
        long capacity = ((initCapacity < 2) ? 2 : initCapacity);
        queuePtr->elements = (void**)P_MALLOC(capacity * sizeof(void*));
        if (queuePtr->elements == NULL) {
            free(queuePtr);
            return NULL;
        }
        queuePtr->pop      = capacity - 1;
        queuePtr->push     = 0;
        queuePtr->capacity = capacity;
    }

    return queuePtr;
}


/* =============================================================================
 * TMqueue_alloc
 * =============================================================================
 */
queue_t*
queue_stm::TMqueue_alloc (TM_ARGDECL  long initCapacity)
{
    queue_t* queuePtr = (queue_t*)TM_MALLOC(sizeof(queue_t));

    if (queuePtr) {
        long capacity = ((initCapacity < 2) ? 2 : initCapacity);
        queuePtr->elements = (void**)TM_MALLOC(capacity * sizeof(void*));
        if (queuePtr->elements == NULL) {
            free(queuePtr);
            return NULL;
        }
        queuePtr->pop      = capacity - 1;
        queuePtr->push     = 0;
        queuePtr->capacity = capacity;
    }

    return queuePtr;
}


/* =============================================================================
 * queue_free
 * =============================================================================
 */
void
queue_stm::queue_free (queue_t* queuePtr)
{
    free(queuePtr->elements);
    free(queuePtr);
}


/* =============================================================================
 * Pqueue_free
 * =============================================================================
 */
void
queue_stm::Pqueue_free (queue_t* queuePtr)
{
    P_FREE(queuePtr->elements);
    P_FREE(queuePtr);
}


/* =============================================================================
 * TMqueue_free
 * =============================================================================
 */
void
queue_stm::TMqueue_free (TM_ARGDECL  queue_t* queuePtr)
{
    void** temp_p = (void**)SLOW_PATH_SHARED_READ_P(queuePtr->elements);
    SLOW_PATH_FREE(temp_p);
    SLOW_PATH_FREE(queuePtr);
}


/* =============================================================================
 * queue_isEmpty
 * =============================================================================
 */
bool_t
queue_stm::queue_isEmpty (queue_t* queuePtr)
{
    long pop      = queuePtr->pop;
    long push     = queuePtr->push;
    long capacity = queuePtr->capacity;

    return (((pop + 1) % capacity == push) ? TRUE : FALSE);
}


/* =============================================================================
 * queue_clear
 * =============================================================================
 */
void
queue_stm::queue_clear (queue_t* queuePtr)
{
    queuePtr->pop  = queuePtr->capacity - 1;
    queuePtr->push = 0;
}


/* =============================================================================
 * TMqueue_isEmpty
 * =============================================================================
 */
bool_t
queue_stm::TMqueue_isEmpty (TM_ARGDECL  queue_t* queuePtr)
{
    long pop      = (long)SLOW_PATH_SHARED_READ(queuePtr->pop);
    long push     = (long)SLOW_PATH_SHARED_READ(queuePtr->push);
    long capacity = (long)SLOW_PATH_SHARED_READ(queuePtr->capacity);

    return (((pop + 1) % capacity == push) ? TRUE : FALSE);
}


/* =============================================================================
 * queue_shuffle
 * =============================================================================
 */
void
queue_stm::queue_shuffle (queue_t* queuePtr, random_t* randomPtr)
{
    long pop      = queuePtr->pop;
    long push     = queuePtr->push;
    long capacity = queuePtr->capacity;

    long numElement;
    if (pop < push) {
        numElement = push - (pop + 1);
    } else {
        numElement = capacity - (pop - push + 1);
    }

    void** elements = queuePtr->elements;
    long i;
    long base = pop + 1;
    for (i = 0; i < numElement; i++) {
        long r1 = random_generate(randomPtr) % numElement;
        long r2 = random_generate(randomPtr) % numElement;
        long i1 = (base + r1) % capacity;
        long i2 = (base + r2) % capacity;
        void* tmp = elements[i1];
        elements[i1] = elements[i2];
        elements[i2] = tmp;
    }
}


/* =============================================================================
 * queue_push
 * =============================================================================
 */
bool_t
queue_stm::queue_push (queue_t* queuePtr, void* dataPtr)
{
    long pop      = queuePtr->pop;
    long push     = queuePtr->push;
    long capacity = queuePtr->capacity;

    assert(pop != push);

    /* Need to resize */
    long newPush = (push + 1) % capacity;
    if (newPush == pop) {

        long newCapacity = capacity * QUEUE_GROWTH_FACTOR;
        void** newElements = (void**)malloc(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return FALSE;
        }

        long dst = 0;
        void** elements = queuePtr->elements;
        if (pop < push) {
            long src;
            for (src = (pop + 1); src < push; src++, dst++) {
                newElements[dst] = elements[src];
            }
        } else {
            long src;
            for (src = (pop + 1); src < capacity; src++, dst++) {
                newElements[dst] = elements[src];
            }
            for (src = 0; src < push; src++, dst++) {
                newElements[dst] = elements[src];
            }
        }

        free(elements);
        queuePtr->elements = newElements;
        queuePtr->pop      = newCapacity - 1;
        queuePtr->capacity = newCapacity;
        push = dst;
        newPush = push + 1; /* no need modulo */
    }

    queuePtr->elements[push] = dataPtr;
    queuePtr->push = newPush;

    return TRUE;
}


/* =============================================================================
 * Pqueue_push
 * =============================================================================
 */
bool_t
queue_stm::Pqueue_push (queue_t* queuePtr, void* dataPtr)
{
    long pop      = queuePtr->pop;
    long push     = queuePtr->push;
    long capacity = queuePtr->capacity;

    assert(pop != push);

    /* Need to resize */
    long newPush = (push + 1) % capacity;
    if (newPush == pop) {

        long newCapacity = capacity * QUEUE_GROWTH_FACTOR;
        void** newElements = (void**)P_MALLOC(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return FALSE;
        }

        long dst = 0;
        void** elements = queuePtr->elements;
        if (pop < push) {
            long src;
            for (src = (pop + 1); src < push; src++, dst++) {
                newElements[dst] = elements[src];
            }
        } else {
            long src;
            for (src = (pop + 1); src < capacity; src++, dst++) {
                newElements[dst] = elements[src];
            }
            for (src = 0; src < push; src++, dst++) {
                newElements[dst] = elements[src];
            }
        }

        P_FREE(elements);
        queuePtr->elements = newElements;
        queuePtr->pop      = newCapacity - 1;
        queuePtr->capacity = newCapacity;
        push = dst;
        newPush = push + 1; /* no need modulo */

    }

    queuePtr->elements[push] = dataPtr;
    queuePtr->push = newPush;

    return TRUE;
}


/* =============================================================================
 * TMqueue_push
 * =============================================================================
 */
bool_t
queue_stm::TMqueue_push (TM_ARGDECL  queue_t* queuePtr, void* dataPtr)
{
    long pop      = (long)SLOW_PATH_SHARED_READ(queuePtr->pop);
    long push     = (long)SLOW_PATH_SHARED_READ(queuePtr->push);
    long capacity = (long)SLOW_PATH_SHARED_READ(queuePtr->capacity);

    assert(pop != push);

    /* Need to resize */
    long newPush = (push + 1) % capacity;
    if (newPush == pop) {
        long newCapacity = capacity * QUEUE_GROWTH_FACTOR;
        void** newElements = (void**)TM_MALLOC(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return FALSE;
        }

        long dst = 0;
        void** elements = (void**)SLOW_PATH_SHARED_READ_P(queuePtr->elements);
        if (pop < push) {
            long src;
            for (src = (pop + 1); src < push; src++, dst++) {
                newElements[dst] = (void*)SLOW_PATH_SHARED_READ_P(elements[src]);
            }
        } else {
            long src;
            for (src = (pop + 1); src < capacity; src++, dst++) {
                newElements[dst] = (void*)SLOW_PATH_SHARED_READ_P(elements[src]);
            }
            for (src = 0; src < push; src++, dst++) {
                newElements[dst] = (void*)SLOW_PATH_SHARED_READ_P(elements[src]);
            }
        }

        SLOW_PATH_FREE(elements);
        SLOW_PATH_SHARED_WRITE_P(queuePtr->elements, newElements);
        SLOW_PATH_SHARED_WRITE(queuePtr->pop,      newCapacity - 1);
        SLOW_PATH_SHARED_WRITE(queuePtr->capacity, newCapacity);
        push = dst;
        newPush = push + 1; /* no need modulo */

    }

    void** elements = (void**)SLOW_PATH_SHARED_READ_P(queuePtr->elements);
    SLOW_PATH_SHARED_WRITE_P(elements[push], dataPtr);
    SLOW_PATH_SHARED_WRITE(queuePtr->push, newPush);

    return TRUE;
}


/* =============================================================================
 * queue_pop
 * =============================================================================
 */
void*
queue_stm::queue_pop (queue_t* queuePtr)
{
    long pop      = queuePtr->pop;
    long push     = queuePtr->push;
    long capacity = queuePtr->capacity;

    long newPop = (pop + 1) % capacity;
    if (newPop == push) {
        return NULL;
    }

    void* dataPtr = queuePtr->elements[newPop];
    queuePtr->pop = newPop;

    return dataPtr;
}


/* =============================================================================
 * TMqueue_pop
 * =============================================================================
 */
void*
queue_stm::TMqueue_pop (TM_ARGDECL  queue_t* queuePtr)
{
    long pop      = (long)SLOW_PATH_SHARED_READ(queuePtr->pop);
    long push     = (long)SLOW_PATH_SHARED_READ(queuePtr->push);
    long capacity = (long)SLOW_PATH_SHARED_READ(queuePtr->capacity);

    long newPop = (pop + 1) % capacity;
    if (newPop == push) {
        return NULL;
    }

    void** elements = (void**)SLOW_PATH_SHARED_READ_P(queuePtr->elements);
    void* dataPtr = (void*)SLOW_PATH_SHARED_READ_P(elements[newPop]);
    SLOW_PATH_SHARED_WRITE(queuePtr->pop, newPop);

    return dataPtr;
}


/* =============================================================================
 *
 * End of queue.c
 *
 * =============================================================================
 */

