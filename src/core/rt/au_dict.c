// This source file is part of the Aument language
// Copyright (c) 2021 the aument contributors
//
// Licensed under Apache License v2.0 with Runtime Library Exception
// See LICENSE.txt for license information
#ifdef AU_IS_INTERPRETER
#include "core/rt/au_dict.h"
#include "core/hash.h"
#include "core/rt/au_struct.h"
#include "core/rt/value.h"
#include "core/value_array.h"
#include "platform/fastdiv.h"
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

static inline au_value_t empty_value() { return au_value_error(); }

static inline int is_empty_value(au_value_t left) {
    return au_value_is_error(left);
}

static inline int value_eq(au_value_t left, au_value_t right) {
    return au_value_get_bool(au_value_eq(left, right));
}

// ** Dictionary internals **
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

/*
 * A general purpose hash table, using the Robin Hood hashing algorithm.
 *
 * Conceptually, it is a hash table using linear probing on lookup with
 * a particular displacement strategy on inserts.  The central idea of
 * the Robin Hood hashing algorithm is to reduce the variance of the
 * probe sequence length (PSL).
 *
 * Reference:
 *
 *	Pedro Celis, 1986, Robin Hood Hashing, University of Waterloo
 *	https://cs.uwaterloo.ca/research/tr/1986/CS-86-14.pdf
 */

#define MAX_GROWTH_STEP (1024U * 1024)

#define APPROX_85_PERCENT(x) (((x)*870) >> 10)
#define APPROX_40_PERCENT(x) (((x)*409) >> 10)

typedef struct {
    au_value_t key;
    au_value_t val;
    uint32_t hash;
    uint32_t psl;
} rh_bucket_t;

struct rhashmap {
    uint32_t size;
    uint32_t nitems;
    uint32_t minsize;
    uint64_t divinfo;
    rh_bucket_t *buckets;

    /*
     * Small optimisation for a single element case: allocate one
     * bucket together with the hashmap structure -- it will generally
     * fit within the same cache-line.
     */
    rh_bucket_t init_bucket;
};

static uint32_t compute_hash(au_value_t key) {
#ifdef AU_USE_NAN_TAGGING
    if(au_value_get_type(key) == AU_VALUE_STR) {
        struct au_string *ptr = au_value_get_string(key);
        return au_hash((uint8_t *)ptr->data, ptr->len);
    }
    return au_hash_u64(key.raw);
#else
    switch (au_value_get_type(key)) {
    case AU_VALUE_DOUBLE: {
        union {
            double d;
            uint64_t u;
        } u;
        u.d = au_value_get_double(key);
        return au_hash_u64(u.u);
    }
    case AU_VALUE_NONE: {
        return 2;
    }
    case AU_VALUE_INT: {
        int32_t value = au_value_get_int(key);
        return au_hash_u32((uint32_t)value);
    }
    case AU_VALUE_BOOL: {
        return au_value_get_bool(key);
    }
    case AU_VALUE_FN: {
        return au_hash_usize(au_value_get_fn(key));
    }
    case AU_VALUE_STR: {
        struct au_string *ptr = au_value_get_string(key);
        return au_hash((uint8_t *)ptr->data, ptr->len);
    }
    case AU_VALUE_STRUCT: {
        return au_hash_usize(au_value_get_struct(key));
    }
    case AU_VALUE_ERROR: {
        AU_UNREACHABLE;
    }
    }
    return 0;
#endif
}

static AU_UNUSED int validate_psl_p(struct rhashmap *hmap,
                                    const rh_bucket_t *bucket,
                                    uint32_t i) {
    uint32_t base_i = fast_rem32(bucket->hash, hmap->size, hmap->divinfo);
    uint32_t diff = (base_i > i) ? hmap->size - base_i + i : i - base_i;
    return au_value_get_type(bucket->key) == AU_VALUE_NONE ||
           diff == bucket->psl;
}

/*
 * rhashmap_get: lookup an value given the key.
 *
 * => If key is present, return its associated value; otherwise NULL.
 */
static au_value_t rhashmap_get(struct rhashmap *hmap, au_value_t key) {
    const uint32_t hash = compute_hash(key);
    uint32_t n = 0, i = fast_rem32(hash, hmap->size, hmap->divinfo);
    rh_bucket_t *bucket;

    /*
     * Lookup is a linear probe.
     */
probe:
    bucket = &hmap->buckets[i];
    // assert(validate_psl_p(hmap, bucket, i));

    if (bucket->hash == hash && value_eq(bucket->key, key)) {
        au_value_ref(bucket->val);
        return bucket->val;
    }

    /*
     * Stop probing if we hit an empty bucket; also, if we hit a
     * bucket with PSL lower than the distance from the base location,
     * then it means that we found the "rich" bucket which should
     * have been captured, if the key was inserted -- see the central
     * point of the algorithm in the insertion function.
     */
    if (is_empty_value(bucket->key) || n > bucket->psl) {
        return empty_value();
    }
    n++;

    /* Continue to the next bucket. */
    i = fast_rem32(i + 1, hmap->size, hmap->divinfo);
    goto probe;
}

/*
 * rhashmap_insert: internal rhashmap_put(), without the resize.
 */
static void rhashmap_insert(struct rhashmap *hmap, au_value_t key,
                            au_value_t val) {
    const uint32_t hash = compute_hash(key);
    rh_bucket_t *bucket, entry;
    uint32_t i;

    /*
     * Setup the bucket entry.
     */
    au_value_ref(key);
    au_value_ref(val);

    entry.key = key;
    entry.val = val;
    entry.hash = hash;
    entry.psl = 0;

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
    if (!is_empty_value(bucket->key)) {
        // assert(validate_psl_p(hmap, bucket, i));

        /*
         * There is a key in the bucket.
         */
        if (bucket->hash == hash && value_eq(bucket->key, key)) {
            /* Duplicate key: return the current value. */
            au_value_deref(entry.key);
            au_value_deref(bucket->val);
            bucket->val = val;
            return;
        }

        /*
         * We found a "rich" bucket.  Capture its location.
         */
        if (entry.psl > bucket->psl) {
            rh_bucket_t tmp;

            /*
             * Place our key-value pair by swapping the "rich"
             * bucket with our entry.  Copy the structures.
             */
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
    *bucket = entry; // copy
    hmap->nitems++;

    // assert(validate_psl_p(hmap, bucket, i));
}

static int rhashmap_resize(struct rhashmap *hmap, size_t newsize) {
    const size_t len = newsize * sizeof(rh_bucket_t);
    rh_bucket_t *oldbuckets = hmap->buckets;
    const size_t oldsize = hmap->size;
    rh_bucket_t *newbuckets;

    // assert(newsize > 0);
    // assert(newsize > hmap->nitems);

    /*
     * Check for an overflow and allocate buckets.  Also, generate
     * a new hash key/seed every time we resize the hash table.
     */
    if (newsize == 1) {
        memset(&hmap->init_bucket, 0, sizeof(rh_bucket_t));
        newbuckets = &hmap->init_bucket;
    } else if (newsize > UINT32_MAX) {
        return -1;
    } else if ((newbuckets = au_data_calloc(1, len)) == NULL) {
        return -1;
    }
    hmap->buckets = newbuckets;
    hmap->size = newsize;
    hmap->nitems = 0;

    for (size_t i = 0; i < newsize; i++) {
        hmap->buckets[i].key = empty_value();
        hmap->buckets[i].val = empty_value();
    }

    hmap->divinfo = fast_div32_init(newsize);

    for (uint32_t i = 0; i < oldsize; i++) {
        const rh_bucket_t *bucket = &oldbuckets[i];

        /* Skip the empty buckets. */
        if (is_empty_value(bucket->key)) {
            continue;
        }
        rhashmap_insert(hmap, bucket->key, bucket->val);
        au_value_deref(bucket->key);
    }
    if (oldbuckets && oldbuckets != &hmap->init_bucket) {
        au_data_free(oldbuckets);
    }
    return 0;
}

/*
 * rhashmap_put: insert a value given the key.
 *
 * => If the key is already present, return its associated value.
 * => Otherwise, on successful insert, return the given value.
 */
static void rhashmap_put(struct rhashmap *hmap, au_value_t key,
                         au_value_t val) {
    const size_t threshold = APPROX_85_PERCENT(hmap->size);

    /*
     * If the load factor is more than the threshold, then resize.
     */
    if (AU_UNLIKELY(hmap->nitems > threshold)) {
        /*
         * Grow the hash table by doubling its size, but with
         * a limit of MAX_GROWTH_STEP.
         */
        const size_t grow_limit = hmap->size + MAX_GROWTH_STEP;
        const size_t newsize = MIN(hmap->size << 1, grow_limit);
        if (rhashmap_resize(hmap, newsize) != 0) {
            au_fatal("out of memory\n"); // TODO
        }
    }

    rhashmap_insert(hmap, key, val);
}

/*
 * rhashmap_del: remove the given key and return its value.
 *
 * => If key was present, return its associated value; otherwise NULL.
 */
static AU_UNUSED au_value_t rhashmap_del(struct rhashmap *hmap,
                                         au_value_t key) {
    const size_t threshold = APPROX_40_PERCENT(hmap->size);
    const uint32_t hash = compute_hash(key);
    uint32_t n = 0, i = fast_rem32(hash, hmap->size, hmap->divinfo);
    rh_bucket_t *bucket;
    au_value_t val = empty_value();

probe:
    /*
     * The same probing logic as in the lookup function.
     */
    bucket = &hmap->buckets[i];
    if (is_empty_value(bucket->key) || n > bucket->psl) {
        return empty_value();
    }
    // assert(validate_psl_p(hmap, bucket, i));

    if (bucket->hash != hash || !value_eq(bucket->key, key)) {
        /* Continue to the next bucket. */
        i = fast_rem32(i + 1, hmap->size, hmap->divinfo);
        n++;
        goto probe;
    }

    /*
     * Free the bucket.
     */
    au_value_deref(bucket->key);
    val = bucket->val;
    bucket->val = empty_value();
    hmap->nitems--;

    /*
     * The probe sequence must be preserved in the deletion case.
     * Use the backwards-shifting method to maintain low variance.
     */
    for (;;) {
        rh_bucket_t *nbucket;

        bucket->key = empty_value();

        i = fast_rem32(i + 1, hmap->size, hmap->divinfo);
        nbucket = &hmap->buckets[i];
        // assert(validate_psl_p(hmap, nbucket, i));

        /*
         * Stop if we reach an empty bucket or hit a key which
         * is in its base (original) location.
         */
        if (is_empty_value(nbucket->key) || nbucket->psl == 0) {
            break;
        }

        nbucket->psl--;
        *bucket = *nbucket;
        bucket = nbucket;
    }

    /*
     * If the load factor is less than threshold, then shrink by
     * halving the size, but not more than the minimum size.
     */
    if (hmap->nitems > hmap->minsize && hmap->nitems < threshold) {
        size_t newsize = MAX(hmap->size >> 1, hmap->minsize);
        (void)rhashmap_resize(hmap, newsize);
    }
    return val;
}

/*
 * rhashmap_create: construct a new hash table.
 *
 * => If size is non-zero, then pre-allocate the given number of buckets;
 * => If size is zero, then a default minimum is used.
 */
static void rhashmap_init(struct rhashmap *hmap, size_t size) {
    memset(hmap, 0, sizeof(struct rhashmap));
    hmap->minsize = MAX(size, 1);
    if (rhashmap_resize(hmap, hmap->minsize) != 0) {
        abort(); // TODO
    }
}

/*
 * rhashmap_destroy: free the memory used by the hash table.
 *
 * => It is the responsibility of the caller to remove elements if needed.
 */
static void rhashmap_destroy(struct rhashmap *hmap) {
    for (uint32_t i = 0; i < hmap->size; i++) {
        const rh_bucket_t *bucket = &hmap->buckets[i];

        if (!is_empty_value(bucket->key)) {
            au_value_deref(bucket->key);
        }
    }
    if (hmap->buckets != &hmap->init_bucket) {
        au_data_free(hmap->buckets);
    }
}

// ** Dictionary API implementation **

struct au_obj_dict {
    struct au_struct header;
    struct rhashmap hashmap;
};

AU_THREAD_LOCAL struct au_struct_vdata au_obj_dict_vdata;
static AU_THREAD_LOCAL int au_obj_dict_vdata_inited = 0;
static void au_obj_dict_vdata_init() {
    if (!au_obj_dict_vdata_inited) {
        au_obj_dict_vdata = (struct au_struct_vdata){
            .del_fn = (au_struct_del_fn_t)au_obj_dict_del,
            .idx_get_fn = (au_struct_idx_get_fn_t)au_obj_dict_get,
            .idx_set_fn = (au_struct_idx_set_fn_t)au_obj_dict_set,
            .len_fn = (au_struct_len_fn_t)au_obj_dict_len,
        };
        au_obj_dict_vdata_inited = 1;
    }
}

struct au_obj_dict *au_obj_dict_new() {
    struct au_obj_dict *obj_dict = au_obj_malloc(
        sizeof(struct au_obj_dict), (au_obj_del_fn_t)au_obj_dict_del);
    au_obj_dict_vdata_init();
    obj_dict->header = (struct au_struct){
        .vdata = &au_obj_dict_vdata,
    };
    rhashmap_init(&obj_dict->hashmap, 1);
    return obj_dict;
}

void au_obj_dict_del(struct au_obj_dict *obj_dict) {
    rhashmap_destroy(&obj_dict->hashmap);
}

int au_obj_dict_get(struct au_obj_dict *obj_dict, const au_value_t key,
                    au_value_t *result) {
    au_value_t get_result = rhashmap_get(&obj_dict->hashmap, key);
    if (is_empty_value(get_result))
        return 0;
    *result = get_result;
    return 1;
}

int au_obj_dict_set(struct au_obj_dict *obj_dict, au_value_t key,
                    au_value_t value) {
    rhashmap_put(&obj_dict->hashmap, key, value);
    return 1;
}

int32_t au_obj_dict_len(struct au_obj_dict *obj_dict) {
    return (int32_t)obj_dict->hashmap.nitems;
}

#ifdef _AUMENT_H
struct au_obj_dict *au_obj_dict_coerce(au_value_t value) {
    if (au_value_get_type(value) != AU_VALUE_STRUCT ||
        au_value_get_struct(value)->vdata != &au_obj_dict_vdata)
        return 0;
    return (struct au_obj_dict *)au_value_get_struct(value);
}
#endif
