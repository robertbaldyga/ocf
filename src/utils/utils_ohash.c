/*
 * Copyright(c) 2022-2025 Huawei Technologies Co., Ltd
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "utils_ohash.h"

#include "ocf_env.h"
#include "ocf/ocf_types.h"

/* For logger */
#include "../ocf_cache_priv.h"
/* For OCF_DIV_ROUND_UP */
#include "../ocf_def_priv.h"

uint64_t ocf_fast_hash_function(size_t range, uint64_t x)
{
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9;
	x = (x ^ (x >> 27)) * 0x94d049bb133111eb;
	x = x ^ (x >> 31);
	return x % range;
}

/* ===========================================================================*/
/* initialize the hash function */
int ocf_ohash_create(ocf_core_t core, struct ocf_ohash64 *hash,
		size_t capacity, char *name)
{
	size_t bucket_count = OCF_DIV_ROUND_UP(capacity, BUCKET_LENGTH);
	size_t alloc_size = bucket_count * sizeof(struct ocf_ohash64_bucket);
	struct ocf_ohash64_bucket *bucket = NULL;
	int i, j;

	bucket = env_aligned_alloc(ENV_PROCESSOR_CACHE_LINE_SIZE, alloc_size);
	if (!bucket)
		return -OCF_ERR_NO_MEM;

	hash->bucket = bucket;
	hash->bucket_count = bucket_count;
	env_strncpy(hash->name, sizeof(hash->name), name,
			env_strnlen(name, sizeof(hash->name)));

	/* Create an array of buckets */
	for (i = 0; i < bucket_count; i++) {
		env_spinlock8_init(&hash->bucket[i].lock);
		for (j = 0; j < BUCKET_LENGTH; j++) {
			hash->bucket[i].items[j] = 0;
			hash->bucket[i].age[j] = j;
		}
	}

	ocf_core_log(core, log_info, "ohash %s capacity %lu\n",
			hash->name, ocf_ohash_get_capacity(hash));

	return 0;
}

/* ===========================================================================*/
void ocf_ohash_destroy(struct ocf_ohash64 *hash)
{
	if (unlikely(hash == NULL || hash->bucket == NULL)) {
		ENV_WARN(true, "NULL Handle (%p) or NULL bucket\n", hash);
		return;
	}

	env_aligned_free(hash->bucket);
	hash->bucket = NULL;
}

/* ===========================================================================*/
/* Find an item location and return it */
uint64_t ocf_ohash_get_locked(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask, bool locked)
{
	struct ocf_ohash64_bucket *bucket = NULL;
	uint64_t masked_item = item & mask, h;
	int i;

	if (unlikely(hash == NULL)) {
		ENV_WARN(true, "hash is NULL\n");
		return (~item & mask);
	}
	h = ocf_fast_hash_function(hash->bucket_count, masked_item);
	bucket = &hash->bucket[h];

	if (locked)
		env_spinlock8_lock(&bucket->lock);

	for (i = 0; i < BUCKET_LENGTH; i++) {
		uint64_t hash_item = *((const volatile uint64_t *)&(bucket->items[i]));
		if ((hash_item & mask) == masked_item) {
			return hash_item;
		}
	}

	return (~item & mask);
}

/* ===========================================================================*/
/* Insert an item to the hash function. */
uint64_t ocf_ohash_set_locked(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask, bool locked)
{
	struct ocf_ohash64_bucket *bucket = NULL;
	int i;
	int item_index = -1;
	uint8_t lru_age = BUCKET_LENGTH;
	uint64_t masked_item = item & mask;
	uint64_t old_item, h;
	uint8_t item_age = 0; /* old value of the item if it already exist, otherwise stays 0 */

	h = ocf_fast_hash_function(hash->bucket_count, masked_item);
	bucket = &hash->bucket[h];

	if (unlikely(hash == NULL)) {
		ENV_WARN(true, "hash is NULL\n");
		return 0;
	}

	/* Locking the bucket */
	if (!locked)
		env_spinlock8_lock(&bucket->lock);

	for (i = 0; i < BUCKET_LENGTH; i++) {
		if (lru_age > bucket->age[i]) {
			lru_age = bucket->age[item_index = i];
		}
		/* check if the item is already in there */
		if ((bucket->items[i] & mask) == masked_item) {
			item_age = bucket->age[item_index = i];
			break;
		}
	}

	old_item = bucket->items[item_index];

	bucket->items[item_index] = item;
	bucket->age[item_index] = BUCKET_LENGTH;

	/* update those who were more recently used than the item to lower position. */
	for (i = 0; i < BUCKET_LENGTH; i++) {
		if (bucket->age[i] > item_age) {
			bucket->age[i]--;
		}
	}
	env_spinlock8_unlock(&bucket->lock);

	return old_item;
}
