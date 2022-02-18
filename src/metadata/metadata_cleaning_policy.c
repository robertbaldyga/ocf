/*
 * copyright(c) 2020 intel corporation
 * spdx-license-identifier: bsd-3-clause-clear
 */

#include "ocf/ocf.h"
#include "metadata.h"
#include "metadata_cleaning_policy.h"
#include "metadata_internal.h"

/*
 * Cleaning policy - Get
 */
struct cleaning_policy_meta *
ocf_metadata_get_cleaning_policy(struct ocf_cache *cache,
		ocf_cache_line_t line)
{
	ENV_BUG_ON(true);
	return NULL;
}
