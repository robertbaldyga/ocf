/*
 * Copyright(c) 2012-2022 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "metadata_raw_pmem.h"
#include "metadata_segment_id.h"
#include "metadata_raw.h"
#include "metadata_io.h"
#include "metadata_raw_atomic.h"
#include "../ocf_def_priv.h"
#include "../ocf_priv.h"

#define OCF_METADATA_RAW_DEBUG 0

#if 1 == OCF_METADATA_RAW_DEBUG
#define OCF_DEBUG_TRACE(cache) \
	ocf_cache_log(log_info, "[Metadata][Raw] %s\n", __func__)

#define OCF_DEBUG_MSG(cache, msg) \
	ocf_cache_log(cache, log_info, "[Metadata][Raw] %s - %s\n", \
			__func__, msg)

#define OCF_DEBUG_PARAM(cache, format, ...) \
	ocf_cache_log(cache, log_info, "[Metadata][Raw] %s - "format"\n", \
			__func__, ##__VA_ARGS__)
#else
#define OCF_DEBUG_TRACE(cache)
#define OCF_DEBUG_MSG(cache, msg)
#define OCF_DEBUG_PARAM(cache, format, ...)
#endif


/*
 * RAM Implementation - Initialize
 */
int _raw_ram_init_pmem(ocf_cache_t cache,
	ocf_flush_page_synch_t lock_page_pfn,
	ocf_flush_page_synch_t unlock_page_pfn,
	struct ocf_metadata_raw *raw)
{
	size_t mem_pool_size;
	int ret;

	OCF_DEBUG_TRACE(cache);

	/* TODO: caller should specify explicitly whether to init mio conc? */
	if (lock_page_pfn) {
		ret = ocf_mio_concurrency_init(&raw->mio_conc,
			raw->ssd_pages_offset, raw->ssd_pages, cache);
		if (ret)
			return ret;
	}

	/* Allocate memory pool for entries */
	mem_pool_size = raw->ssd_pages;
	mem_pool_size *= PAGE_SIZE;
	raw->mem_pool_limit = mem_pool_size;
	raw->mem_pool = env_secure_alloc_pmem(mem_pool_size);
	if (!raw->mem_pool) {
		ocf_mio_concurrency_deinit(&raw->mio_conc);
		return -OCF_ERR_NO_MEM;
	}
	ENV_BUG_ON(env_memset(raw->mem_pool, mem_pool_size, 0));

	raw->lock_page = lock_page_pfn;
	raw->unlock_page = unlock_page_pfn;

	return 0;
}


int _raw_ram_deinit_pmem(ocf_cache_t cache,
		struct ocf_metadata_raw *raw)
{
	OCF_DEBUG_TRACE(cache);

	if (raw->mem_pool) {
		env_secure_free_pmem(raw->mem_pool, raw->mem_pool_limit);
		raw->mem_pool = NULL;
	}

	ocf_mio_concurrency_deinit(&raw->mio_conc);

	return 0;
}
