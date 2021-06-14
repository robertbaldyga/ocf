/*
 * Copyright(c) 2012-2021 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#ifndef __LAYER_EVICTION_POLICY_H__
#define __LAYER_EVICTION_POLICY_H__

#include "ocf/ocf.h"
#include "lru.h"
#include "lru_structs.h"

#define OCF_PENDING_EVICTION_LIMIT 512UL

#define OCF_NUM_EVICTION_LISTS 32

struct ocf_part;
struct ocf_request;

struct eviction_policy {
	union {
		struct lru_eviction_policy lru;
	} policy;
};

/* Eviction policy metadata per cache line */
union eviction_policy_meta {
	struct lru_eviction_policy_meta lru;
} __attribute__((packed));

/* the caller must hold the metadata lock for all operations
 *
 * For range operations the caller can:
 * set core_id to -1 to purge the whole cache device
 * set core_id to -2 to purge the whole cache partition
 */
struct eviction_policy_ops {
	void (*init_cline)(ocf_cache_t cache, ocf_cache_line_t cline);
	void (*rm_cline)(ocf_cache_t cache,
			ocf_cache_line_t cline);
	bool (*can_evict)(ocf_cache_t cache);
	uint32_t (*req_clines)(struct ocf_request *req, struct ocf_part *part,
			uint32_t cline_no);
	void (*hot_cline)(ocf_cache_t cache, ocf_cache_line_t cline);
	void (*init_evp)(ocf_cache_t cache, struct ocf_part *part);
	void (*dirty_cline)(ocf_cache_t cache,
			struct ocf_part *part,
			uint32_t cline_no);
	void (*clean_cline)(ocf_cache_t cache,
			struct ocf_part *part,
			uint32_t cline_no);
	void (*flush_dirty)(ocf_cache_t cache, struct ocf_user_part *user_part,
			ocf_queue_t io_queue, uint32_t count);
	const char *name;
};

extern struct eviction_policy_ops evict_policy_ops[ocf_eviction_max];

/*
 * Deallocates space according to eviction priorities.
 *
 * @returns:
 * 'LOOKUP_HIT' if evicted enough cachelines to serve @req
 * 'LOOKUP_MISS' otherwise
 */
int space_managment_evict_do(struct ocf_request *req);

int space_management_free(ocf_cache_t cache, uint32_t count);

#endif
