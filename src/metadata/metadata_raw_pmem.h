/*
 * Copyright(c) 2012-2021 Intel Corporation
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __METADATA_RAW_PMEM_H__
#define __METADATA_RAW_PMEM_H__

#include <ocf/ocf_types.h>
#include "metadata_raw.h"

int _raw_ram_init_pmem(ocf_cache_t cache,
	ocf_flush_page_synch_t lock_page_pfn,
	ocf_flush_page_synch_t unlock_page_pfn,
	struct ocf_metadata_raw *raw);


int _raw_ram_deinit_pmem(ocf_cache_t cache,
    struct ocf_metadata_raw *raw);

#endif /*METADATA_RAW_PMEM_H_*/
