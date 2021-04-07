// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rt/malloc.h"
#include "hash.h"
#include "hm_vars.h"
#include "platform/fastdiv.h"
#include "platform/platform.h"
#include "rt/exception.h"
#endif

//
// The following code was adopted from rhashmap, with extra modifications
// to integrate it with Aument
//
// Source: https://github.com/rmind/rhashmap
//
/*-
 * Copyright (c) 2017-2020 Mindaugas Rasiukevicius <rmind at noxt eu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#define MAX_GROWTH_STEP (1024U * 1024)
#define APPROX_85_PERCENT(x) (((x)*870) >> 10)
#define APPROX_40_PERCENT(x) (((x)*409) >> 10)
#define NEW_KEY ((size_t)-1)

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

static int AU_UNUSED
validate_psl_p(struct au_hm_vars *hmap,
               const struct au_hm_var_el_bucket *bucket, uint32_t i) {
    uint32_t base_i = fast_rem32(bucket->hash, hmap->size, hmap->divinfo);
    uint32_t diff = (base_i > i) ? hmap->size - base_i + i : i - base_i;
    return bucket->key_len == 0 || diff == bucket->psl;
}

/*
 * hm_get: lookup an value given the key.
 *
 * => If key is present, return its associated value; otherwise NULL.
 */
const au_hm_var_value_t *au_hm_vars_get(const struct au_hm_vars *hmap,
                                        const char *key, size_t len) {
    if (AU_UNLIKELY(!hmap->initialized))
        au_fatal("using uninitialized au_hm_vars");

    const uint32_t hash = au_hash((const uint8_t *)key, len);
    uint32_t n = 0, i = fast_rem32(hash, hmap->size, hmap->divinfo);
    struct au_hm_var_el_bucket *bucket;

    // assert(key != NULL);
    // assert(len != 0);

    /*
     * Lookup is a linear probe.
     */
probe:
    bucket = &hmap->buckets[i];
    // assert(validate_psl_p(hmap, bucket, i));

    if (bucket->hash == hash && bucket->key_len == len &&
        memcmp(&hmap->key_bytes[bucket->key_idx], key, len) == 0) {
        return &bucket->value;
    }

    /*
     * Stop probing if we hit an empty bucket; also, if we hit a
     * bucket with PSL lower than the distance from the base location,
     * then it means that we found the "rich" bucket which should
     * have been captured, if the key was inserted -- see the central
     * point of the algorithm in the insertion function.
     */
    if (bucket->key_len == 0 || n > bucket->psl) {
        return NULL;
    }
    n++;

    /* Continue to the next bucket. */
    i = fast_rem32(i + 1, hmap->size, hmap->divinfo);
    goto probe;
}

static void initialize_entry(struct au_hm_var_el_bucket *entry,
                             struct au_hm_vars *hmap, const char *key,
                             size_t len) {
    const size_t key_idx = hmap->key_bytes_len;
    hmap->key_bytes =
        au_data_realloc(hmap->key_bytes, hmap->key_bytes_len + len);
    memcpy(&hmap->key_bytes[hmap->key_bytes_len], key, len);
    hmap->key_bytes_len += len;

    entry->key_idx = key_idx;
    entry->key_len = len;
}

/*
 * hm_insert: internal hm_put(), without the resize.
 */
static au_hm_var_value_t *hm_insert(struct au_hm_vars *hmap,
                                    size_t key_idx, size_t key_len,
                                    au_hm_var_value_t value,
                                    const char *key_ptr) {
    const uint32_t hash = au_hash((const uint8_t *)key_ptr, key_len);
    struct au_hm_var_el_bucket *bucket, entry;
    uint32_t i;

    /*
     * Setup the bucket entry.
     */
    entry.key_idx = key_idx;
    entry.key_len = key_len;
    entry.hash = hash;
    entry.value = value;
    entry.psl = 0;

    int entry_initialized = 1;
    if(key_idx == NEW_KEY)
        entry_initialized = 0;

    /*
     * From the paper: "when inserting, if a record probes a location
     * that is already occupied, the record that has traveled longer
     * in its probe sequence keeps the location, and the other one
     * continues on its probe sequence" (page 12).
     *
     * Basically: if the probe sequence length (PSL) of the element
     * being inserted is greater than PSL of the element in the bucket,
     * then swap them and continue.
     */
    i = fast_rem32(hash, hmap->size, hmap->divinfo);
probe:
    bucket = &hmap->buckets[i];
    if (bucket->key_len != 0) {
        // assert(validate_psl_p(hmap, bucket, i));

        /*
         * There is a key in the bucket.
         */
        if (bucket->hash == hash && bucket->key_len == key_len &&
            memcmp(&hmap->key_bytes[bucket->key_idx], key_ptr, key_len) ==
                0) {
            /* Duplicate key: return the current value. */
            return &bucket->value;
        }

        /*
         * We found a "rich" bucket.  Capture its location.
         */
        if (entry.psl > bucket->psl) {
            struct au_hm_var_el_bucket tmp;

            /*
             * Place our key-value pair by swapping the "rich"
             * bucket with our entry.  Copy the structures.
             */
            if (!entry_initialized) {
                initialize_entry(&entry, hmap, key_ptr, key_len);
                entry_initialized = 1;
            }
            tmp = entry;
            entry = *bucket;
            *bucket = tmp;
        }
        entry.psl++;

        /* Continue to the next bucket. */
        // assert(validate_psl_p(hmap, bucket, i));
        i = fast_rem32(i + 1, hmap->size, hmap->divinfo);
        goto probe;
    }

    /*
     * Found a free bucket: insert the entry.
     */
    if (!entry_initialized) {
        initialize_entry(&entry, hmap, key_ptr, key_len);
        entry_initialized = 1;
    }
    *bucket = entry; // copy
    hmap->nitems++;

    // assert(validate_psl_p(hmap, bucket, i));
    return 0;
}

static int hm_resize(struct au_hm_vars *hmap, size_t newsize) {
    const size_t len = newsize * sizeof(struct au_hm_var_el_bucket);
    struct au_hm_var_el_bucket *oldbuckets = hmap->buckets;
    const size_t oldsize = hmap->size;
    struct au_hm_var_el_bucket *newbuckets;

    // assert(newsize > 0);
    // assert(newsize > hmap->nitems);

    /*
     * Check for an overflow and allocate buckets.  Also, generate
     * a new hash key/seed every time we resize the hash table.
     */
    if (newsize > UINT_MAX) {
        return -1;
    } else if ((newbuckets = au_data_calloc(1, len)) == NULL) {
        return -1;
    }
    hmap->buckets = newbuckets;
    hmap->size = newsize;
    hmap->nitems = 0;

    hmap->divinfo = fast_div32_init(newsize);

    for (size_t i = 0; i < oldsize; i++) {
        const struct au_hm_var_el_bucket *bucket = &oldbuckets[i];

        /* Skip the empty buckets. */
        if (bucket->key_len == 0) {
            continue;
        }
        hm_insert(hmap, bucket->key_idx, bucket->key_len, bucket->value,
                  &hmap->key_bytes[bucket->key_idx]);
    }
    if (oldbuckets) {
        au_data_free(oldbuckets);
    }
    return 0;
}

/*
 * hm_put: insert a value given the key.
 *
 * => If the key is already present, return its associated value.
 * => Otherwise, on successful insert, return the given value.
 */
au_hm_var_value_t *au_hm_vars_add(struct au_hm_vars *hmap, const char *key,
                                  size_t len, au_hm_var_value_t value) {
    if (AU_UNLIKELY(!hmap->initialized))
        au_hm_vars_init(hmap);

    const size_t threshold = APPROX_85_PERCENT(hmap->size);

    /*
     * If the load factor is more than the threshold, then resize.
     */
    if (AU_UNLIKELY(hmap->nitems > threshold)) {
        /*
         * Grow the hash table by doubling its size, but with
         * a limit of   .
         */
        const size_t grow_limit = hmap->size + MAX_GROWTH_STEP;
        const size_t newsize = MIN(hmap->size << 1, grow_limit);
        if (hm_resize(hmap, newsize) != 0) {
            return NULL;
        }
    }

    return hm_insert(hmap, NEW_KEY, len, value, key);
}

/*
 * hm_create: construct a new hash table.
 *
 * => If size is non-zero, then pre-allocate the given number of buckets;
 * => If size is zero, then a default minimum is used.
 */
void au_hm_vars_init(struct au_hm_vars *hmap) {
    if (!hmap->initialized) {
        memset(hmap, 0, sizeof(struct au_hm_vars));
        hmap->initialized = 1;
        hmap->minsize = 1;
        if (AU_UNLIKELY(hm_resize(hmap, hmap->minsize) != 0 ||
                        hmap->buckets == 0 || hmap->size == 0))
            au_fatal("unable to initialize hm_vars %p", hmap);
    }
}

/*
 * hm_destroy: free the memory used by the hash table.
 *
 * => It is the responsibility of the caller to remove elements if needed.
 */
void au_hm_vars_del(struct au_hm_vars *hmap) {
    au_data_free(hmap->buckets);
    au_data_free(hmap->key_bytes);
}
