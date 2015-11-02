/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* mu_reduce_level.c: Reduce level overwriting root with it's star-record-only child block.  */

#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsblkops.h"
#include "gdskill.h"
#include "gdscc.h"
#include "jnl.h"
#include "copy.h"
#include "muextr.h"
#include "mu_reorg.h"

/* Include prototypes */
#include "t_write.h"
#include "mupip_reorg.h"

GBLREF gv_namehead	*gv_target;
GBLREF sgmnt_data_ptr_t cs_data;
GBLREF uint4		update_array_size;	/* for the BLK_* macros */
GBLREF char		*update_array, *update_array_ptr;
GBLREF unsigned int     t_tries;
GBLREF uint4		t_err;

/**************************************************************************************
	Input Parameters:
		(GBL_DEF) gv_target: For reference block's history
	Output Parameters:
		kill_set_ptr : List of blocks to be freed
 ***************************************************************************************/
enum cdb_sc mu_reduce_level(kill_set *kill_set_ptr)
{
	int		level;
	int		old_blk_sz;
	int		blk_seg_cnt, blk_size;
	blk_segment	*bs_ptr1, *bs_ptr2;
	sm_uc_ptr_t 	old_blk_base, save_blk;

	level = gv_target->hist.depth;
	if (1 == level)
		return cdb_sc_oprnotneeded;

 	blk_size = cs_data->blk_size;
	kill_set_ptr->used = 0;
	memset(kill_set_ptr, 0, SIZEOF(kill_set));
	CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
	old_blk_base = gv_target->hist.h[level].buffaddr;
	old_blk_sz = ((blk_hdr_ptr_t)(old_blk_base))->bsiz;
	if (SIZEOF(blk_hdr) + BSTAR_REC_SIZE != old_blk_sz)
		return cdb_sc_oprnotneeded;
	old_blk_base = gv_target->hist.h[level-1].buffaddr;
	old_blk_sz = ((blk_hdr_ptr_t)(old_blk_base))->bsiz;
	BLK_ADDR(save_blk, old_blk_sz - SIZEOF(blk_hdr), unsigned char);
	memcpy(save_blk, old_blk_base + SIZEOF(blk_hdr), old_blk_sz - SIZEOF(blk_hdr));
	BLK_INIT(bs_ptr2, bs_ptr1);
    	BLK_SEG(bs_ptr2, save_blk, old_blk_sz - SIZEOF(blk_hdr));
	if (!BLK_FINI(bs_ptr2, bs_ptr1))
	{
		assert(t_tries < CDB_STAGNATE);
		return cdb_sc_blkmod;
	}
	t_write(&gv_target->hist.h[level], (unsigned char *)bs_ptr1, 0, 0, level - 1, TRUE, TRUE, GDS_WRITE_KILLTN);
	kill_set_ptr->blk[kill_set_ptr->used].flag = 0;
	kill_set_ptr->blk[kill_set_ptr->used].level = 0;
	kill_set_ptr->blk[kill_set_ptr->used++].block = gv_target->hist.h[level-1].blk_num;
	return cdb_sc_normal;
}
