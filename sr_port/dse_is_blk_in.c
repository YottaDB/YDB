/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "dsefind.h"
#include "dse.h"

GBLREF sgmnt_addrs	*cs_addrs;
GBLREF short int	patch_dir_path_count;
GBLREF block_id		patch_find_blk;
GBLREF bool		patch_find_root_search;
GBLREF int4		patch_offset[MAX_BT_DEPTH + 1];
GBLREF int4		patch_offset1[MAX_BT_DEPTH + 1];
GBLREF block_id		patch_path[MAX_BT_DEPTH + 1];
GBLREF block_id		patch_path1[MAX_BT_DEPTH + 1];
GBLREF short int	patch_path_count;

int dse_is_blk_in(sm_uc_ptr_t rp, sm_uc_ptr_t r_top, short size)
{
	sm_uc_ptr_t	key_top;
	char		targ_key[MAX_KEY_SZ + 1];

	memcpy(targ_key, rp + SIZEOF(rec_hdr), size);
	if ((patch_find_blk != patch_path[0])
		&& !dse_order(patch_path[0], &patch_path[1], patch_offset, targ_key, size, 0))
			return FALSE;
	patch_dir_path_count = patch_path_count;
	if (!patch_find_root_search || (patch_find_blk != patch_path[patch_path_count - 1]))
	{
		if ((0 >= patch_path[patch_path_count - 1]) || (patch_path[patch_path_count] > cs_addrs->ti->total_blks))
			return FALSE;
		patch_find_root_search = FALSE;
		for (key_top = rp + SIZEOF(rec_hdr); (*key_top++ || *key_top++) && (key_top < r_top);)
			;
		size = key_top - rp - SIZEOF(rec_hdr);
		if (0 > size)
			size = 0;
		else if (SIZEOF(targ_key) < size)
			size = SIZEOF(targ_key);
		memcpy(targ_key, rp + SIZEOF(rec_hdr), size);
		patch_path1[0] = patch_path[patch_path_count - 1];
		patch_path[patch_path_count - 1] = 0;
		patch_path_count = 1;
		if ((patch_find_blk != patch_path1[0])
			&& !dse_order(patch_path1[0], &patch_path1[1], patch_offset1, targ_key, size, 0))
				return FALSE;
	} else
		patch_path_count = 0;
	return TRUE;
}
