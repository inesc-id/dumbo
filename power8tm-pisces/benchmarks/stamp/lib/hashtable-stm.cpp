/* =============================================================================
 *
 * hashtable.c
 *
 * =============================================================================
 *
 * Copyright (C) Stanford University, 2006.  All Rights Reserved.
 * Author: Chi Cao Minh
 *
 * =============================================================================
 *
 * Options:
 *
 * LIST_NO_DUPLICATES (default: allow duplicates)
 *
 * HASHTABLE_RESIZABLE (enable dynamically increasing number of buckets)
 *
 * HASHTABLE_SIZE_FIELD (size is explicitely stored in
 *     hashtable and not implicitly defined by the sizes of
 *     all bucket lists => more conflicts in case of parallel access)
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
#include "hashtable-stm.h"
#include "types.h"

using namespace hashtable_stm;

/* =============================================================================
 * hashtable_iter_reset
 * =============================================================================
 */
void
hashtable_stm::hashtable_iter_reset (hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    itPtr->bucket = 0;
    list_iter_reset(&(itPtr->it), hashtablePtr->buckets[0]);
}


/* =============================================================================
 * TMhashtable_iter_reset
 * =============================================================================
 */
void
hashtable_stm::TMhashtable_iter_reset (TM_ARGDECL
                        hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    itPtr->bucket = 0;
    list_stm::TMlist_iter_reset(TM_ARG &(itPtr->it), hashtablePtr->buckets[0]);
}


/* =============================================================================
 * hashtable_iter_hasNext
 * =============================================================================
 */
bool_t
hashtable_stm::hashtable_iter_hasNext (hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = buckets[bucket];
        if (list_iter_hasNext(&it, chainPtr)) {
            return TRUE;
        }
        /* May use dummy bucket; see allocBuckets() */
        list_iter_reset(&it, buckets[++bucket]);
    }

    return FALSE;
}


/* =============================================================================
 * hashtable_iter_hasNext
 * =============================================================================
 */
bool_t
hashtable_stm::TMhashtable_iter_hasNext (TM_ARGDECL
                          hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = buckets[bucket];
        if (list_stm::TMlist_iter_hasNext(TM_ARG &it, chainPtr)) {
            return TRUE;
        }
        /* May use dummy bucket; see allocBuckets() */
        list_stm::TMlist_iter_reset(TM_ARG &it, buckets[++bucket]);
    }

    return FALSE;
}


/* =============================================================================
 * hashtable_iter_next
 * =============================================================================
 */
void*
hashtable_stm::hashtable_iter_next (hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;
    void* dataPtr = NULL;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = hashtablePtr->buckets[bucket];
        if (list_iter_hasNext(&it, chainPtr)) {
            pair_t* pairPtr = (pair_t*)list_iter_next(&it, chainPtr);
            dataPtr = pairPtr->secondPtr;
            break;
        }
        /* May use dummy bucket; see allocBuckets() */
        list_iter_reset(&it, buckets[++bucket]);
    }

    itPtr->bucket = bucket;
    itPtr->it = it;

    return dataPtr;
}


/* =============================================================================
 * TMhashtable_iter_next
 * =============================================================================
 */
void*
hashtable_stm::TMhashtable_iter_next (TM_ARGDECL
                       hashtable_iter_t* itPtr, hashtable_t* hashtablePtr)
{
    long bucket;
    long numBucket = hashtablePtr->numBucket;
    list_t** buckets = hashtablePtr->buckets;
    list_iter_t it = itPtr->it;
    void* dataPtr = NULL;

    for (bucket = itPtr->bucket; bucket < numBucket; /* inside body */) {
        list_t* chainPtr = hashtablePtr->buckets[bucket];
        if (list_stm::TMlist_iter_hasNext(TM_ARG &it, chainPtr)) {
            pair_t* pairPtr = (pair_t*)list_stm::TMlist_iter_next(TM_ARG &it, chainPtr);
            dataPtr = pairPtr->secondPtr;
            break;
        }
        /* May use dummy bucket; see allocBuckets() */
        list_stm::TMlist_iter_reset(TM_ARG &it, buckets[++bucket]);
    }

    itPtr->bucket = bucket;
    itPtr->it = it;

    return dataPtr;
}


/* =============================================================================
 * allocBuckets
 * -- Returns NULL on error
 * =============================================================================
 */
static list_t**
allocBuckets (long numBucket, long (*comparePairs)(const pair_t*, const pair_t*))
{
    long i;
    list_t** buckets;

    /* Allocate bucket: extra bucket is dummy for easier iterator code */
    buckets = (list_t**)malloc((numBucket + 1) * sizeof(list_t*));
    if (buckets == NULL) {
        return NULL;
    }

    for (i = 0; i < (numBucket + 1); i++) {
        list_t* chainPtr =
            list_alloc((long (*)(const void*, const void*))comparePairs);
        if (chainPtr == NULL) {
            while (--i >= 0) {
                list_free(buckets[i]);
            }
            return NULL;
        }
        buckets[i] = chainPtr;
    }

    return buckets;
}


/* =============================================================================
 * TMallocBuckets
 * -- Returns NULL on error
 * =============================================================================
 */
static list_t**
TMallocBuckets (TM_ARGDECL
                long numBucket, long (*comparePairs)(const pair_t*, const pair_t*))
{
    long i;
    list_t** buckets;

    /* Allocate bucket: extra bucket is dummy for easier iterator code */
    buckets = (list_t**)TM_MALLOC((numBucket + 1) * sizeof(list_t*));
    if (buckets == NULL) {
        return NULL;
    }

    for (i = 0; i < (numBucket + 1); i++) {
        list_t* chainPtr =
            list_stm::TMlist_alloc(TM_ARG (long (*)(const void*, const void*))comparePairs);
        if (chainPtr == NULL) {
            while (--i >= 0) {
                list_stm::TMlist_free(TM_ARG buckets[i]);
            }
            return NULL;
        }
        buckets[i] = chainPtr;
    }

    return buckets;
}


/* =============================================================================
 * hashtable_alloc
 * -- Returns NULL on failure
 * -- Negative values for resizeRatio or growthFactor select default values
 * =============================================================================
 */
hashtable_t*
hashtable_stm::hashtable_alloc (long initNumBucket,
                 ulong_t (*hash)(const void*),
                 long (*comparePairs)(const pair_t*, const pair_t*),
                 long resizeRatio,
                 long growthFactor)
{
    hashtable_t* hashtablePtr;

    hashtablePtr = (hashtable_t*)malloc(sizeof(hashtable_t));
    if (hashtablePtr == NULL) {
        return NULL;
    }

    hashtablePtr->buckets = allocBuckets(initNumBucket, comparePairs);
    if (hashtablePtr->buckets == NULL) {
        free(hashtablePtr);
        return NULL;
    }

    hashtablePtr->numBucket = initNumBucket;
#ifdef HASHTABLE_SIZE_FIELD
    hashtablePtr->size = 0;
#endif
    hashtablePtr->hash = hash;
    hashtablePtr->comparePairs = comparePairs;
    hashtablePtr->resizeRatio = ((resizeRatio < 0) ?
                                  HASHTABLE_DEFAULT_RESIZE_RATIO : resizeRatio);
    hashtablePtr->growthFactor = ((growthFactor < 0) ?
                                  HASHTABLE_DEFAULT_GROWTH_FACTOR : growthFactor);

    return hashtablePtr;
}


/* =============================================================================
 * TMhashtable_alloc
 * -- Returns NULL on failure
 * -- Negative values for resizeRatio or growthFactor select default values
 * =============================================================================
 */
hashtable_t*
hashtable_stm::TMhashtable_alloc (TM_ARGDECL
                   long initNumBucket,
                   ulong_t (*hash)(const void*),
                   long (*comparePairs)(const pair_t*, const pair_t*),
                   long resizeRatio,
                   long growthFactor)
{
    hashtable_t* hashtablePtr;

    hashtablePtr = (hashtable_t*)TM_MALLOC(sizeof(hashtable_t));
    if (hashtablePtr == NULL) {
        return NULL;
    }

    hashtablePtr->buckets = TMallocBuckets(TM_ARG  initNumBucket, comparePairs);
    if (hashtablePtr->buckets == NULL) {
        SLOW_PATH_FREE(hashtablePtr);
        return NULL;
    }

    hashtablePtr->numBucket = initNumBucket;
#ifdef HASHTABLE_SIZE_FIELD
    hashtablePtr->size = 0;
#endif
    hashtablePtr->hash = hash;
    hashtablePtr->comparePairs = comparePairs;
    hashtablePtr->resizeRatio = ((resizeRatio < 0) ?
                                  HASHTABLE_DEFAULT_RESIZE_RATIO : resizeRatio);
    hashtablePtr->growthFactor = ((growthFactor < 0) ?
                                  HASHTABLE_DEFAULT_GROWTH_FACTOR : growthFactor);

    return hashtablePtr;
}


/* =============================================================================
 * freeBuckets
 * =============================================================================
 */
static void
freeBuckets (list_t** buckets, long numBucket)
{
    long i;

    for (i = 0; i < numBucket; i++) {
        list_free(buckets[i]);
    }

    free(buckets);
}


/* =============================================================================
 * TMfreeBuckets
 * =============================================================================
 */
static void
TMfreeBuckets (TM_ARGDECL  list_t** buckets, long numBucket)
{
    long i;

    for (i = 0; i < numBucket; i++) {
        list_stm::TMlist_free(TM_ARG buckets[i]);
    }

    SLOW_PATH_FREE(buckets);
}


/* =============================================================================
 * hashtable_free
 * =============================================================================
 */
void
hashtable_stm::hashtable_free (hashtable_t* hashtablePtr)
{
    freeBuckets(hashtablePtr->buckets, hashtablePtr->numBucket);
    free(hashtablePtr);
}


/* =============================================================================
 * TMhashtable_free
 * =============================================================================
 */
void
hashtable_stm::TMhashtable_free (TM_ARGDECL  hashtable_t* hashtablePtr)
{
    TMfreeBuckets(TM_ARG  hashtablePtr->buckets, hashtablePtr->numBucket);
    SLOW_PATH_FREE(hashtablePtr);
}


/* =============================================================================
 * hashtable_isEmpty
 * =============================================================================
 */
bool_t
hashtable_stm::hashtable_isEmpty (hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return ((hashtablePtr->size == 0) ? TRUE : FALSE);
#else
    long i;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        if (!list_isEmpty(hashtablePtr->buckets[i])) {
            return FALSE;
        }
    }

    return TRUE;
#endif
}


/* =============================================================================
 * TMhashtable_isEmpty
 * =============================================================================
 */
bool_t
hashtable_stm::TMhashtable_isEmpty (TM_ARGDECL  hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return ((TM_SHARED_READ(hashtablePtr->size) == 0) ? TRUE : FALSE);
#else
    long i;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        if (!list_stm::TMlist_isEmpty(TM_ARG hashtablePtr->buckets[i])) {
            return FALSE;
        }
    }

    return TRUE;
#endif
}


/* =============================================================================
 * hashtable_getSize
 * -- Returns number of elements in hash table
 * =============================================================================
 */
long
hashtable_stm::hashtable_getSize (hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return hashtablePtr->size;
#else
    long i;
    long size = 0;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        size += list_getSize(hashtablePtr->buckets[i]);
    }

    return size;
#endif
}


/* =============================================================================
 * TMhashtable_getSize
 * -- Returns number of elements in hash table
 * =============================================================================
 */
long
hashtable_stm::TMhashtable_getSize (TM_ARGDECL  hashtable_t* hashtablePtr)
{
#ifdef HASHTABLE_SIZE_FIELD
    return (long)TM_SHARED_READ(hashtablePtr->size);
#else
    long i;
    long size = 0;

    for (i = 0; i < hashtablePtr->numBucket; i++) {
        size += list_stm::TMlist_getSize(TM_ARG hashtablePtr->buckets[i]);
    }

    return size;
#endif
}


/* =============================================================================
 * hashtable_containsKey
 * =============================================================================
 */
bool_t
hashtable_stm::hashtable_containsKey (hashtable_t* hashtablePtr, void* keyPtr)
{
    long i = hashtablePtr->hash(keyPtr) % hashtablePtr->numBucket;
    pair_t* pairPtr;
    pair_t findPair;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_find(hashtablePtr->buckets[i], &findPair);

    return ((pairPtr != NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * TMhashtable_containsKey
 * =============================================================================
 */
bool_t
hashtable_stm::TMhashtable_containsKey (TM_ARGDECL  hashtable_t* hashtablePtr, void* keyPtr)
{
    long i = hashtablePtr->hash(keyPtr) % hashtablePtr->numBucket;
    pair_t* pairPtr;
    pair_t findPair;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_stm::TMlist_find(TM_ARG hashtablePtr->buckets[i], &findPair);

    return ((pairPtr != NULL) ? TRUE : FALSE);
}


/* =============================================================================
 * hashtable_find
 * -- Returns NULL on failure, else pointer to data associated with key
 * =============================================================================
 */
void*
hashtable_stm::hashtable_find (hashtable_t* hashtablePtr, void* keyPtr)
{
    long i = hashtablePtr->hash(keyPtr) % hashtablePtr->numBucket;
    pair_t* pairPtr;
    pair_t findPair;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_find(hashtablePtr->buckets[i], &findPair);
    if (pairPtr == NULL) {
        return NULL;
    }

    return pairPtr->secondPtr;
}


/* =============================================================================
 * TMhashtable_find
 * -- Returns NULL on failure, else pointer to data associated with key
 * =============================================================================
 */
void*
hashtable_stm::TMhashtable_find (TM_ARGDECL  hashtable_t* hashtablePtr, void* keyPtr)
{
    long i = hashtablePtr->hash(keyPtr) % hashtablePtr->numBucket;
    pair_t* pairPtr;
    pair_t findPair;

    findPair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_stm::TMlist_find(TM_ARG hashtablePtr->buckets[i], &findPair);
    if (pairPtr == NULL) {
        return NULL;
    }

    return pairPtr->secondPtr;
}


/* =============================================================================
 * hashtable_insert
 * =============================================================================
 */
bool_t
hashtable_stm::hashtable_insert (hashtable_t* hashtablePtr, void* keyPtr, void* dataPtr)
{
    long numBucket = hashtablePtr->numBucket;
    long i = hashtablePtr->hash(keyPtr) % numBucket;
#if defined(HASHTABLE_SIZE_FIELD) || defined(HASHTABLE_RESIZABLE)
    long newSize;
#endif

    pair_t findPair;
    findPair.firstPtr = keyPtr;
    pair_t* pairPtr = (pair_t*)list_find(hashtablePtr->buckets[i], &findPair);
    if (pairPtr != NULL) {
        return FALSE;
    }

    pair_t* insertPtr = pair_alloc(keyPtr, dataPtr);
    if (insertPtr == NULL) {
        return FALSE;
    }

#ifdef HASHTABLE_SIZE_FIELD
    newSize = hashtablePtr->size + 1;
    assert(newSize > 0);
#elif defined(HASHTABLE_RESIZABLE)
    newSize = hashtable_getSize(hashtablePtr) + 1;
    assert(newSize > 0);
#endif

    /* Add new entry  */
    if (list_insert(hashtablePtr->buckets[i], insertPtr) == FALSE) {
        pair_free(insertPtr);
        return FALSE;
    }
#ifdef HASHTABLE_SIZE_FIELD
    hashtablePtr->size = newSize;
#endif

    return TRUE;
}


/* =============================================================================
 * TMhashtable_insert
 * =============================================================================
 */
bool_t
hashtable_stm::TMhashtable_insert (TM_ARGDECL
                    hashtable_t* hashtablePtr, void* keyPtr, void* dataPtr)
{
    long numBucket = hashtablePtr->numBucket;
    long i = hashtablePtr->hash(keyPtr) % numBucket;

    pair_t findPair;
    findPair.firstPtr = keyPtr;
    pair_t* pairPtr = (pair_t*)list_stm::TMlist_find(TM_ARG hashtablePtr->buckets[i], &findPair);
    if (pairPtr != NULL) {
        return FALSE;
    }

    pair_t* insertPtr = pair_stm::TMpair_alloc(TM_ARG keyPtr, dataPtr);
    if (insertPtr == NULL) {
        return FALSE;
    }

    /* Add new entry  */
    if (list_stm::TMlist_insert(TM_ARG hashtablePtr->buckets[i], insertPtr) == FALSE) {
        pair_stm::TMpair_free(TM_ARG insertPtr);
        return FALSE;
    }

#ifdef HASHTABLE_SIZE_FIELD
    long newSize = TM_SHARED_READ(hashtablePtr->size) + 1;
    assert(newSize > 0);
    TM_SHARED_WRITE(hashtablePtr->size, newSize);
#endif

    return TRUE;
}


/* =============================================================================
 * hashtable_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
bool_t
hashtable_stm::hashtable_remove (hashtable_t* hashtablePtr, void* keyPtr)
{
    long numBucket = hashtablePtr->numBucket;
    long i = hashtablePtr->hash(keyPtr) % numBucket;
    list_t* chainPtr = hashtablePtr->buckets[i];
    pair_t* pairPtr;
    pair_t removePair;

    removePair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_find(chainPtr, &removePair);
    if (pairPtr == NULL) {
        return FALSE;
    }

    bool_t status = list_remove(chainPtr, &removePair);
    assert(status);
    pair_free(pairPtr);

#ifdef HASHTABLE_SIZE_FIELD
    hashtablePtr->size--;
    assert(hashtablePtr->size >= 0);
#endif

    return TRUE;
}


/* =============================================================================
 * TMhashtable_remove
 * -- Returns TRUE if successful, else FALSE
 * =============================================================================
 */
bool_t
hashtable_stm::TMhashtable_remove (TM_ARGDECL  hashtable_t* hashtablePtr, void* keyPtr)
{
    long numBucket = hashtablePtr->numBucket;
    long i = hashtablePtr->hash(keyPtr) % numBucket;
    list_t* chainPtr = hashtablePtr->buckets[i];
    pair_t* pairPtr;
    pair_t removePair;

    removePair.firstPtr = keyPtr;
    pairPtr = (pair_t*)list_stm::TMlist_find(TM_ARG chainPtr, &removePair);
    if (pairPtr == NULL) {
        return FALSE;
    }

    bool_t status = list_stm::TMlist_remove(TM_ARG chainPtr, &removePair);
    assert(status);
    pair_stm::TMpair_free(TM_ARG pairPtr);

#ifdef HASHTABLE_SIZE_FIELD
    TM_SHARED_WRITE(hashtablePtr->size
                    (long)TM_SHARED_READ(hashtablePtr->size)-1);
    assert(hashtablePtr->size >= 0);
#endif

    return TRUE;
}
