/****************************************************************
 *								*
 * Copyright (c) 2013-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gvcst_protos.h"	/* needed by OPEN_BASEREG_IF_STATSREG */

/* Searches a global directory map array for which map entry an input "key" falls in.
 * "key" could be an unsubscripted or subscripted global reference.
 * "skip_basedb_open" is set to TRUE in a special case from a call in "gvcst_init_statsDB" and is FALSE otherwise.
 *	In that special case, the caller knows to set the ^%YGS map entry to point to the appropriate statsdb region
 *	so we do not need to do unnecessary opens of other basedb regions. But otherwise this function ensures that
 *	if ever a map entry is returned that points to a statsdb region, the corresponding basedb region has been opened.
 */
gd_binding *gv_srch_map(gd_addr *addr, char *key, int key_len, boolean_t skip_basedb_open)
{
	int		res;
	int		low, high, mid;
	gd_binding	*map_start, *map;
#	ifdef DEBUG
	int		dbg_match;
#	endif

	map_start = addr->maps;
	assert(('%' == map_start[1].gvkey.addr[0]) && (1 == map_start[1].gvname_len));
	/* We expect all callers to search for global names that start with "^%" or higher. */
	assert(0 <= memcmp(key, &(map_start[1].gvkey.addr[0]), key_len));
	low = 2;	/* get past local locks AND first map entry which is always "%" */
	high = addr->n_maps - 1;
	DEBUG_ONLY(dbg_match = -1;)
	/* At all times in the loop, "low" corresponds to the smallest possible value for "map"
	 * and "high" corresponds to the highest possible value for "map".
	 */
	do
	{
		if (low == high)
		{
			assert((-1 == dbg_match) || (low == dbg_match));
			map = &map_start[low];
			if (!skip_basedb_open)
				OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
			return map;
		}
		assert(low < high);
		mid = (low + high) / 2;
		assert(low <= mid);
		assert(mid < high);
		map = &map_start[mid];
		res = memcmp(key, &(map->gvkey.addr[0]), key_len);
		if (0 > res)
			high = mid;
		else if (0 < res)
			low = mid + 1;
		else if (key_len < (map->gvkey_len - 1))
			high = mid;
		else
		{
			assert(key_len == (map->gvkey_len - 1));
			low = mid + 1;
#			ifdef DEBUG
			dbg_match = low;
#			else
			map = &map_start[low];
			if (!skip_basedb_open)
				OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
			return map;
#			endif
		}
	} while (TRUE);
}

/* Similar to gv_srch_map except that it does a linear search starting at a specific map and going FORWARD.
 * We expect this function to be invoked in case the caller expects the target map to be found very close to the current map.
 * This might be faster in some cases than a binary search of the entire gld map array (done by gv_srch_map).
 */
gd_binding *gv_srch_map_linear(gd_binding *start_map, char *key, int key_len)
{
	gd_binding	*map;
	int		res;
#	ifdef DEBUG
	gd_addr		*addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	map = start_map;
	DEBUG_ONLY(
		addr = TREF(gd_targ_addr);
		assert(map > addr->maps);
	)
	for ( ; ; map++)
	{
		/* Currently, the only callers of this function are gvcst_spr_* functions (e.g. "gvcst_spr_data" etc.).
		 * And most of them (except gvcst_spr_query/gvcst_start_queryget) start an implicit TP transaction right
		 * after this call. And since statsDB init is deferred once in TP, it is preferable to do the init before
		 * the TP begins. So we include the OPEN_BASEREG_IF_STATSREG macro call here instead of in each of the
		 * caller. This can be moved back to the individual callers if new callers of this function happen which
		 * don't need this macro.
		 */
		OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
		assert(map < &addr->maps[addr->n_maps]);
		res = memcmp(key, &map->gvkey.addr[0], key_len);
		if (0 < res)
			continue;
		if (0 > res)
			break;
		/* res == 0 at this point */
		if (key_len < (map->gvkey_len - 1))
			break;
		assert(key_len == (map->gvkey_len - 1));
		map++;
		break;
	}
	OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
	return map;
}

/* Similar to gv_srch_map_linear except that it does the linear search going BACKWARD. */
gd_binding *gv_srch_map_linear_backward(gd_binding *start_map, char *key, int key_len)
{
	gd_binding	*map;
	int		res;
#	ifdef DEBUG
	gd_addr		*addr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	map = start_map;
	DEBUG_ONLY(
		addr = TREF(gd_targ_addr);
		assert(map < &addr->maps[addr->n_maps]);
	)
	for ( ; ; map--)
	{
		/* Currently, the only caller of this function is "gvcst_spr_zprevious". And it starts an implicit TP transaction
		 * right after this call. And since statsDB init is deferred once in TP, it is preferable to do the init before
		 * the TP begins. So we include the OPEN_BASEREG_IF_STATSREG macro call here instead of in each of the
		 * caller. This can be moved back to the individual callers if new callers of this function happen which
		 * don't need this macro.
		 */
		OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
		assert(map >= addr->maps);
		res = memcmp(key, &map->gvkey.addr[0], key_len);
		if (0 < res)
			break;
		if (0 > res)
			continue;
		/* res == 0 at this point */
		if (key_len < (map->gvkey_len - 1))
			continue;
		assert(key_len == (map->gvkey_len - 1));
		break;
	}
	map++;
	assert(map > addr->maps);
	OPEN_BASEREG_IF_STATSREG(map);	/* can modify map->reg.addr if statsDBReg */
	return map;
}
