/* =============================================================================
 *
 * heap.c
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


#include <stdlib.h>
#include <assert.h>
#include "heap-htm.h"
#include "tm.h"
#include "types.h"

using namespace heap_htm;

/* =============================================================================
 * DECLARATION OF TM_CALLABLE FUNCTIONS
 * =============================================================================
 */

TM_CALLABLE
static void
TMsiftUp (TM_ARGDECL  heap_t* heapPtr, long startIndex);

TM_CALLABLE
static void
TMheapify (TM_ARGDECL  heap_t* heapPtr, long startIndex);


/* =============================================================================
 * heap_alloc
 * -- Returns NULL on failure
 * =============================================================================
 */
heap_t*
heap_htm::heap_alloc (long initCapacity, long (*compare)(const void*, const void*))
{
    heap_t* heapPtr;

    heapPtr = (heap_t*)malloc(sizeof(heap_t));
    if (heapPtr) {
        long capacity = ((initCapacity > 0) ? (initCapacity) : (1));
        heapPtr->elements = (void**)malloc(capacity * sizeof(void*));
        assert(heapPtr->elements);
        heapPtr->size = 0;
        heapPtr->capacity = capacity;
        heapPtr->compare = compare;
    }

    return heapPtr;
}


/* =============================================================================
 * heap_free
 * =============================================================================
 */
void
heap_htm::heap_free (heap_t* heapPtr)
{
    free(heapPtr->elements);
    free(heapPtr);
}


/* =============================================================================
 * siftUp
 * =============================================================================
 */
static void
siftUp (heap_t* heapPtr, long startIndex)
{
    void** elements = heapPtr->elements;
    long (*compare)(const void*, const void*) = heapPtr->compare;

    long index = startIndex;
    while ((index > 1)) {
        long parentIndex = PARENT(index);
        void* parentPtr = elements[parentIndex];
        void* thisPtr   = elements[index];
        if (compare(parentPtr, thisPtr) >= 0) {
            break;
        }
        void* tmpPtr = parentPtr;
        elements[parentIndex] = thisPtr;
        elements[index] = tmpPtr;
        index = parentIndex;
    }
}


/* =============================================================================
 * TMsiftUp
 * =============================================================================
 */
static void
TMsiftUp (TM_ARGDECL  heap_t* heapPtr, long startIndex)
{
    void** elements = (void**)FAST_PATH_SHARED_READ_P(heapPtr->elements);
    long (*compare)(const void*, const void*) = heapPtr->compare;

    long index = startIndex;
    while ((index > 1)) {
        long parentIndex = PARENT(index);
        void* parentPtr = (void*)FAST_PATH_SHARED_READ_P(elements[parentIndex]);
        void* thisPtr   = (void*)FAST_PATH_SHARED_READ_P(elements[index]);
        if (compare(parentPtr, thisPtr) >= 0) {
            break;
        }
        void* tmpPtr = parentPtr;
        FAST_PATH_SHARED_WRITE_P(elements[parentIndex], thisPtr);
        FAST_PATH_SHARED_WRITE_P(elements[index], tmpPtr);
        index = parentIndex;
    }
}


/* =============================================================================
 * heap_insert
 * -- Returns FALSE on failure
 * =============================================================================
 */
bool_t
heap_htm::heap_insert (heap_t* heapPtr, void* dataPtr)
{
    long size = heapPtr->size;
    long capacity = heapPtr->capacity;

    if ((size + 1) >= capacity) {
        long newCapacity = capacity * 2;
        void** newElements = (void**)malloc(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return FALSE;
        }
        heapPtr->capacity = newCapacity;
        long i;
        void** elements = heapPtr->elements;
        for (i = 0; i <= size; i++) {
            newElements[i] = elements[i];
        }
        free(heapPtr->elements);
        heapPtr->elements = newElements;
    }

    size = ++(heapPtr->size);
    heapPtr->elements[size] = dataPtr;
    siftUp(heapPtr, size);

    return TRUE;
}


/* =============================================================================
 * TMheap_insert
 * -- Returns FALSE on failure
 * =============================================================================
 */
bool_t
heap_htm::TMheap_insert (TM_ARGDECL  heap_t* heapPtr, void* dataPtr)
{
    long size = (long)FAST_PATH_SHARED_READ(heapPtr->size);
    long capacity = (long)FAST_PATH_SHARED_READ(heapPtr->capacity);

    if ((size + 1) >= capacity) {
        long newCapacity = capacity * 2;
        void** newElements = (void**)TM_MALLOC(newCapacity * sizeof(void*));
        if (newElements == NULL) {
            return FALSE;
        }
        FAST_PATH_SHARED_WRITE(heapPtr->capacity, newCapacity);
        long i;
        void** elements = FAST_PATH_SHARED_READ_P(heapPtr->elements);
        for (i = 0; i <= size; i++) {
            newElements[i] = (void*)FAST_PATH_SHARED_READ_P(elements[i]);
        }
        FAST_PATH_FREE(heapPtr->elements);
        FAST_PATH_SHARED_WRITE(heapPtr->elements, newElements);
    }

    size++;
    FAST_PATH_SHARED_WRITE(heapPtr->size, size);
    void** elements = (void**)FAST_PATH_SHARED_READ_P(heapPtr->elements);
    FAST_PATH_SHARED_WRITE_P(elements[size], dataPtr);
    TMsiftUp(TM_ARG  heapPtr, size);

    return TRUE;
}


/* =============================================================================
 * heapify
 * =============================================================================
 */
static void
heapify (heap_t* heapPtr, long startIndex)
{
    void** elements = heapPtr->elements;
    long (*compare)(const void*, const void*) = heapPtr->compare;

    long size = heapPtr->size;
    long index = startIndex;

    while (1) {

        long leftIndex = LEFT_CHILD(index);
        long rightIndex = RIGHT_CHILD(index);
        long maxIndex = -1;

        if ((leftIndex <= size) &&
            (compare(elements[leftIndex], elements[index]) > 0))
        {
            maxIndex = leftIndex;
        } else {
            maxIndex = index;
        }

        if ((rightIndex <= size) &&
            (compare(elements[rightIndex], elements[maxIndex]) > 0))
        {
            maxIndex = rightIndex;
        }

        if (maxIndex == index) {
            break;
        } else {
            void* tmpPtr = elements[index];
            elements[index] = elements[maxIndex];
            elements[maxIndex] = tmpPtr;
            index = maxIndex;
        }
    }
}


/* =============================================================================
 * TMheapify
 * =============================================================================
 */
static void
TMheapify (TM_ARGDECL  heap_t* heapPtr, long startIndex)
{
    void** elements = (void**)FAST_PATH_SHARED_READ_P(heapPtr->elements);
    long (*compare)(const void*, const void*) = heapPtr->compare;

    long size = (long)FAST_PATH_SHARED_READ(heapPtr->size);
    long index = startIndex;

    while (1) {

        long leftIndex = LEFT_CHILD(index);
        long rightIndex = RIGHT_CHILD(index);
        long maxIndex = -1;

        if ((leftIndex <= size) &&
            (compare((void*)FAST_PATH_SHARED_READ_P(elements[leftIndex]),
                     (void*)FAST_PATH_SHARED_READ_P(elements[index])) > 0))
        {
            maxIndex = leftIndex;
        } else {
            maxIndex = index;
        }

        if ((rightIndex <= size) &&
            (compare((void*)FAST_PATH_SHARED_READ_P(elements[rightIndex]),
                     (void*)FAST_PATH_SHARED_READ_P(elements[maxIndex])) > 0))
        {
            maxIndex = rightIndex;
        }

        if (maxIndex == index) {
            break;
        } else {
            void* tmpPtr = (void*)FAST_PATH_SHARED_READ_P(elements[index]);
            FAST_PATH_SHARED_WRITE_P(elements[index],
                              (void*)FAST_PATH_SHARED_READ(elements[maxIndex]));
            FAST_PATH_SHARED_WRITE_P(elements[maxIndex], tmpPtr);
            index = maxIndex;
        }
    }
}



/* =============================================================================
 * heap_remove
 * -- Returns NULL if empty
 * =============================================================================
 */
void*
heap_htm::heap_remove (heap_t* heapPtr)
{
    long size = heapPtr->size;

    if (size < 1) {
        return NULL;
    }

    void** elements = heapPtr->elements;
    void* dataPtr = elements[1];
    elements[1] = elements[size];
    heapPtr->size = size - 1;
    heapify(heapPtr, 1);

    return dataPtr;
}


/* =============================================================================
 * TMheap_remove
 * -- Returns NULL if empty
 * =============================================================================
 */
void*
heap_htm::TMheap_remove (TM_ARGDECL  heap_t* heapPtr)
{
    long size = (long)FAST_PATH_SHARED_READ(heapPtr->size);

    if (size < 1) {
        return NULL;
    }

    void** elements = (void**)FAST_PATH_SHARED_READ_P(heapPtr->elements);
    void* dataPtr = (void*)FAST_PATH_SHARED_READ_P(elements[1]);
    FAST_PATH_SHARED_WRITE_P(elements[1], FAST_PATH_SHARED_READ_P(elements[size]));
    FAST_PATH_SHARED_WRITE(heapPtr->size, (size - 1));
    TMheapify(TM_ARG  heapPtr, 1);

    return dataPtr;
}


/* =============================================================================
 * heap_isValid
 * =============================================================================
 */
bool_t
heap_htm::heap_isValid (heap_t* heapPtr)
{
    long size = heapPtr->size;
    long (*compare)(const void*, const void*) = heapPtr->compare;
    void** elements = heapPtr->elements;

    long i;
    for (i = 1; i < size; i++) {
        if (compare(elements[i+1], elements[PARENT(i+1)]) > 0) {
            return FALSE;
        }
    }

    return TRUE;
}
