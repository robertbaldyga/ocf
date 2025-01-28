/*
 * Copyright(c) 2022-2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __OCF_PREFETCH_PRIV_H__
#define __OCF_PREFETCH_PRIV_H__

#include "ocf/ocf_types.h"
#include "../ocf_request.h"

#define OCF_PF_ID_MAX_BITS	3
#define OCF_PF_ID_MAX		((1 << OCF_PF_ID_MAX_BITS) - 1)

#define MAX_TOTAL_PF		(8 * MiB)
#define MAX_SINGLE_PF(len)	(OCF_MAX(64 * KiB, len))

typedef enum {
	ocf_pf_mask_none = 0,
	ocf_pf_mask_readahead = 1 << ocf_prefetch_readahead,
	ocf_pf_mask_stream = 1 << ocf_prefetch_stream,
} ocf_pf_mask_t;

typedef void *ocf_pf_t;

typedef struct {
	uint64_t addr;
	uint32_t len;
	ocf_prefetch_t pf_id;
} ocf_pf_req_info_t;

#define __VALID_PF_MASK (ocf_pf_mask_readahead | ocf_pf_mask_stream)

#define OCF_PF_MASK_FIRST ((__VALID_PF_MASK != 0) ? \
		__builtin_ctz((unsigned int)(__VALID_PF_MASK)) : 0)

#define OCF_PF_MASK_LAST ((__VALID_PF_MASK != 0) ? \
		31 - __builtin_clz((unsigned int)(__VALID_PF_MASK)) : 0)

#define OCF_PF_ID_VALID(pf_id) ((pf_id) < ocf_prefetch_max)

#define OCF_PF_ID_ENABLED(pf_id, enabled_mask) \
	((1 << ((int)(pf_id))) & enabled_mask)

#define for_each_pf_id(pf_id) \
	for (pf_id = OCF_PF_MASK_FIRST; pf_id <= OCF_PF_MASK_LAST; pf_id++)

#define for_each_valid_pf_id(pf_id) \
	for_each_pf_id(pf_id) \
		if (OCF_PF_ID_ENABLED(pf_id, __VALID_PF_MASK))

#define for_each_enabled_pf_id(pf_id, enabled_mask) \
	for_each_pf_id(pf_id) \
		if (OCF_PF_ID_ENABLED(pf_id, enabled_mask & __VALID_PF_MASK))

int ocf_prefetch_create(ocf_core_t core);
void ocf_prefetch_destroy(ocf_core_t core);
void ocf_prefetch(struct ocf_request *req);

#endif /* __OCF_PREFETCH_PRIV_H__ */
