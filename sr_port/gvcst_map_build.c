/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdsbml.h"

#include "send_msg.h"		/* prototypes */
#include "gvcst_map_build.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	boolean_t	dse_running;

void gvcst_map_build(uint4 *array, sm_uc_ptr_t base_addr, cw_set_element *cs, trans_num ctn)
{
	boolean_t	busy, status;
	uint4		ret, (*bml_func)();

	VALIDATE_BM_BLK(cs->blk, (blk_hdr_ptr_t)base_addr, cs_addrs, gv_cur_region, status);
	if (!status)
		GTMASSERT;	/* it is not a valid bitmap block */
	((blk_hdr_ptr_t)base_addr)->tn = ctn;
	base_addr += sizeof(blk_hdr);
	bml_func = (busy = (cs->reference_cnt > 0)) ? bml_busy : (cs_addrs->hdr->db_got_to_v5_once ? bml_recycled : bml_free);
	while (*array)
	{
		if (0 == (ret = (* bml_func)((*array - cs->blk), base_addr)))
			cs->reference_cnt--;
		array++;
	}
}
