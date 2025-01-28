/*
 * Copyright(c) 2022-2025 Huawei Technologies Co., Ltd
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __OCF_OHASH_H__
#define __OCF_OHASH_H__

#include "ocf/ocf_types.h"
#include "ocf_env.h"

#define OCF_OHASH_NAME_LEN	32
#define BUCKET_LENGTH		7

struct ocf_ohash64_bucket {
	uint64_t items[BUCKET_LENGTH];
	uint8_t age[BUCKET_LENGTH];
	env_atomic8 lock;
};

struct ocf_ohash64 {
	struct ocf_ohash64_bucket *bucket;
	size_t bucket_count;
	char name[OCF_OHASH_NAME_LEN];
};

int ocf_ohash_create(ocf_core_t core, struct ocf_ohash64 *hash,
		size_t capacity, char *name);
void ocf_ohash_destroy(struct ocf_ohash64 *hash);
uint64_t ocf_ohash_get_locked(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask, bool locked);
uint64_t ocf_ohash_set_locked(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask, bool locked);

static inline uint64_t ocf_ohash_get(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask)
{
	return ocf_ohash_get_locked(hash, item, mask, false);
}

static inline uint64_t ocf_ohash_set(struct ocf_ohash64 *hash, uint64_t item,
		uint64_t mask)
{
	return ocf_ohash_set_locked(hash, item, mask, false);
}

static inline size_t ocf_ohash_get_capacity(struct ocf_ohash64 *hash)
{
	return hash->bucket_count * BUCKET_LENGTH;
}

#endif	/* __OCF_OHASH_H__ */
