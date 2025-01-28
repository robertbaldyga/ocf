/*
 * Copyright(c) 2022-2025 Huawei Technologies Co., Ltd.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "../ocf_core_priv.h"
#include "ocf_prefetch_priv.h"

#include "ocf_env.h"
#include "ocf_prefetch_priv.h"
#include "ocf_prefetch_stream.h"
#include "ocf_prefetch_naive.h"
#include "../engine/engine_prefetch.h"

#include "../metadata/metadata.h"
#include "../utils/utils_cache_line.h"
#include "../ocf_cache_priv.h"
#include "../ocf_core_priv.h"
#include "../ocf_def_priv.h"
#include "../ocf_request.h"

/* ===========================================================================*/

#define OCF_PREFETCH_DEBUG 0

#if 1 == OCF_PREFETCH_DEBUG
#define OCF_DEBUG_TRACE(core) \
       ocf_core_log(core, log_info, "[Prefetch] %s\n", __func__)

#define OCF_DEBUG_PARAM(core, format, ...) \
       ocf_core_log(core, log_info, "[Prefetch] %s(%d) - "format"\n", \
                       __func__, __LINE__, ##__VA_ARGS__)
#else
#define OCF_DEBUG_TRACE(core)
#define OCF_DEBUG_PARAM(core, format, ...)
#endif

/* ===========================================================================*/

typedef struct {
	ocf_pf_req_info_t req_parts[ocf_prefetch_max];
	uint8_t num_parts;
} ocf_pf_req_parts_info_t;

/* ===========================================================================*/
static void prefetch_complete(struct ocf_request *req, int error)
{
	ocf_req_put(req);
}

/* ===========================================================================*/
/* Call the relevant Prefetch algorithm and get the prefetch info */
static void get_prefetch_info(ocf_core_t core, struct ocf_request *req,
					ocf_pf_req_parts_info_t *req_parts_info)
{
	static void (*get_info[])(ocf_core_t core,
			ocf_pf_req_info_t *req_info) = {
		ocf_pf_readahead_get_info,
		ocf_pf_stream_get_info,
	};
	ocf_prefetch_t pf_id;

	/* initialize output: no valid results yet */
	req_parts_info->num_parts = 0;

	for_each_enabled_pf_id(pf_id, core->prefetch_mask) {
		/* initialize input to prefetchers as triggering request's address and length */
		ocf_pf_req_info_t *req_info = &req_parts_info->req_parts[req_parts_info->num_parts];
		req_info->addr = req->addr;
		req_info->len  = req->bytes;
		req_info->pf_id = ocf_prefetch_max;

		/* call prefetcher */
		get_info[pf_id](core, req_info);
		if (!OCF_PF_ID_VALID(req_info->pf_id))
			continue;

		req_parts_info->num_parts++;
	}
}

static bool is_hit(ocf_cache_t cache, struct ocf_request *req, uint64_t addr)
{
	return ocf_metadata_is_hit_no_lock(cache, ocf_core_get_id(req->core),
			ocf_bytes_2_lines(cache, addr));
}

/* ===========================================================================*/
/* Update the request address and size to contain only a heuristic sequence */
/* of data that isn't in the cache */
#define MAX_SKIP_CL	(8)
static uint32_t get_pf_req_info(struct ocf_request *req,
		uint64_t *byte_position, uint32_t len, uint32_t maxlen)
{
	ocf_cache_t cache = req->cache;
	uint64_t addr;
	uint32_t byte_length = ocf_cache_get_line_size(cache);
	uint64_t first_addr = *byte_position;
	uint64_t last_addr = first_addr + len;
	uint64_t first_miss_addr = 0, last_miss_addr = 0;	/* 0 can't be a valid prefetch address */

	int64_t skip = ocf_bytes_2_lines(cache, OCF_MIN(len, maxlen)) - 1;
	skip = OCF_MIN(skip, MAX_SKIP_CL);
	skip = OCF_MAX(skip, 1);
	skip = ocf_lines_2_bytes(cache, skip);

	for (addr = first_addr; addr < last_addr; addr += skip) {
		if (is_hit(cache, req, addr)) {
			if (first_miss_addr)
				break;
		} else {
			if (!first_miss_addr) {
				first_miss_addr = addr;
				last_addr = OCF_MIN(last_addr, first_miss_addr + maxlen);
			}

			last_miss_addr = addr;
		}
	}

	if (first_miss_addr) {
		if ((first_miss_addr - first_addr) > byte_length) {
			/* Need to find the correct first miss address */
			*byte_position = first_miss_addr - skip + byte_length;
			len = skip - byte_length;
			first_miss_addr -= get_pf_req_info(req, byte_position, len, len);
			last_addr = OCF_MIN(last_addr, first_miss_addr + maxlen);
			last_miss_addr = OCF_MIN(last_miss_addr, last_addr - byte_length);
		}

		if ((last_addr - last_miss_addr) > byte_length) {
			/* Need to find the correct last miss address */
			*byte_position = last_miss_addr + byte_length;
			len = last_addr - *byte_position;
			last_miss_addr += get_pf_req_info(req, byte_position, len, len);
		}

		*byte_position = first_miss_addr;
		return last_miss_addr + byte_length - first_miss_addr;
	}

	return 0;
}

/* ===========================================================================*/
/* Create the prefetch database per core */
int ocf_prefetch_create(ocf_core_t core)
{
	return ocf_pf_stream_create(core);
}

/* ===========================================================================*/
/* Destroy the prefetch database per core */
void ocf_prefetch_destroy(ocf_core_t core)
{
	ocf_pf_stream_destroy(core);
}

void ocf_prefetch_part(struct ocf_request *req, ocf_pf_req_info_t *req_info)
{
	uint64_t byte_position;
	struct ocf_request *prefetch_req = NULL;
	uint64_t last_addr;
	uint32_t len = 0, total_len = 0;
	uint32_t maxlen = ocf_lines_2_bytes(req->cache, ocf_bytes_round_lines(req->cache, MAX_SINGLE_PF(req->bytes)));

	/* round address down to whole line size */
	req_info->addr = ocf_lines_2_bytes(req->cache, ocf_bytes_round_lines(req->cache, req_info->addr));
	/* round length up to whole line size */
	req_info->len = ocf_lines_2_bytes(req->cache, ocf_bytes_2_lines_round_up(req->cache, req_info->len));

	/* Abort if request exceeds backend volume size */
	if (unlikely(req_info->addr >= ocf_volume_get_length(&req->core->volume))) {
		return;
	}

	/* Trim the len not to exceed backend volume size */
	req_info->len = OCF_MIN(req_info->len,
			ocf_volume_get_length(&req->core->volume) - req_info->addr);

	OCF_DEBUG_PARAM(req->core, "pf_id=%u,addr=%lu,len=%d",
			req_info->pf_id, req_info->addr, req_info->len);

	byte_position = req_info->addr;
	last_addr = req_info->addr + req_info->len;

	while (byte_position < last_addr) {
		if ((len = get_pf_req_info(req, &byte_position, last_addr - byte_position, maxlen)) == 0)
			break;

		prefetch_req = ocf_req_new_extended(req->io_queue, req->core,
				byte_position, len, OCF_READ);

		if (unlikely(prefetch_req == NULL)) {
			ENV_WARN(true, "ocf_new_req(addr = 0x%p, len = 0x%x) failed\n",
					(void *)byte_position, len);
			break;
		}
		prefetch_req->io.io_class = req->io.io_class;
		prefetch_req->flags = req->flags;
		prefetch_req->io.pf_id = req_info->pf_id;

		prefetch_req->complete = prefetch_complete;

		ocf_prefetch_read(prefetch_req);

		total_len += len;
		if (total_len >= MAX_TOTAL_PF)
			break;

		byte_position += len;
		maxlen = OCF_MIN(maxlen, ocf_lines_2_bytes(req->cache, ocf_bytes_round_lines(req->cache, MAX_TOTAL_PF - total_len)));
	}
}

void ocf_prefetch(struct ocf_request *req)
{
	int i;
	ocf_pf_req_parts_info_t req_parts_info;

	ENV_BUILD_BUG_ON(OCF_PF_ID_MAX <= OCF_PF_MASK_LAST);


	/* Return if the request isn't a candidate for prefetch */
	if (req->rw != OCF_READ || OCF_PF_ID_VALID(req->io.pf_id)) {
		return;
	}

	/* Get the prefetch info */
	get_prefetch_info(req->core, req, &req_parts_info);

	/* process suggestions and trigger prefetch-core-read requests */
	for (i = 0; i < req_parts_info.num_parts; i++)
		ocf_prefetch_part(req, &req_parts_info.req_parts[i]);
}
