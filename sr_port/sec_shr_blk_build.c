/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "sec_shr_blk_build.h"
#include "repl_msg.h"		/* needed for jnlpool_addrs_ptr_t */
#include "gtmsource.h"		/* needed for jnlpool_addrs_ptr_t */
#include "secshr_db_clnup.h"

void	sec_shr_blk_build(sgmnt_addrs *csa, sgmnt_data_ptr_t csd, cw_set_element *cse, sm_uc_ptr_t base_addr, trans_num ctn)
{
	blk_segment	*seg, *stop_ptr, *array;
	unsigned char	*ptr;

	array = (blk_segment *)cse->upd_addr;
	assert(csa->read_write);
	/* block transaction number needs to be modified first. see comment in gvcst_blk_build as to why */
	((blk_hdr_ptr_t)base_addr)->bver = GDSVCURR;
	assert(csa->now_crit || (ctn < csd->trans_hist.curr_tn));
	assert(!csa->now_crit || (ctn == csd->trans_hist.curr_tn));
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	assert(array->len);
	((blk_hdr_ptr_t)base_addr)->bsiz = UINTCAST(array->len);
	((blk_hdr_ptr_t)base_addr)->levl = cse->level;

	if (cse->forward_process)
	{
		ptr = base_addr + SIZEOF(blk_hdr);
		for (seg = array + 1, stop_ptr = (blk_segment *)array->addr;  seg <= stop_ptr;  seg++)
		{
			if (!seg->len)
				continue;
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, seg);
			memmove(ptr, seg->addr, seg->len);
			ptr += seg->len;
		}
	} else
	{
		ptr = base_addr + array->len;
		for  (seg = (blk_segment*)array->addr, stop_ptr = array;  seg > stop_ptr;  seg--)
		{
			if (!seg->len)
				continue;
			ptr -= seg->len;
			DBG_BG_PHASE2_CHECK_CR_IS_PINNED(csa, seg);
			memmove(ptr, seg->addr, seg->len);
		}
	}
}
