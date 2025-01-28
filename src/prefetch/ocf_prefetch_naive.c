/*
 * Copyright(c) 2022-2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ocf_prefetch_naive.h"
#include "ocf/ocf_def.h"

/* ===========================================================================*/
/* Read-ahead prefetchers */
void ocf_pf_readahead_get_info(ocf_core_t core, ocf_pf_req_info_t *req_info)
{
	req_info->pf_id = ocf_prefetch_readahead;
	req_info->addr += req_info->len;
}

/* ===========================================================================*/
