/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "t_write_map.h"

GBLREF cw_set_element	cw_set[];
GBLREF unsigned char	cw_set_depth;
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgm_info		*sgm_info_ptr;
GBLREF short		dollar_tlevel;

void t_write_map (
		  block_id 	blk,		/*  block number being written */
		  sm_uc_ptr_t	old_addr,	/* address of before image of the block */
		  unsigned char	*upd_addr,	/* list of blocks to be cleared in bit map */
		  trans_num	tn)
{
	cw_set_element	*cs;

	if (blk >= cs_addrs->ti->total_blks)
		GTMASSERT;
	if (dollar_tlevel == 0)
	{
		assert(cw_set_depth < CDB_CW_SET_SIZE);
		cs = &cw_set[cw_set_depth++];
	} else
	{
		tp_cw_list(&cs);
		sgm_info_ptr->cw_set_depth++;
	}
	cs->mode = gds_t_writemap;
	cs->blk = blk;
	cs->old_block = old_addr;
	cs->ins_off = 0;
	cs->index = 0;
	cs->reference_cnt = 0;
	cs->upd_addr = upd_addr;
	cs->tn = tn;
	cs->level = LCL_MAP_LEVL;
	cs->write_type = GDS_WRITE_PLAIN;
}
