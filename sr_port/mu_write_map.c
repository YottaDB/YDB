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
#include "muextr.h"
#include "mupip_reorg.h"

GBLREF cw_set_element cw_set[];
GBLREF unsigned char cw_set_depth;
GBLREF unsigned char cw_map_depth;
GBLREF sgmnt_addrs *cs_addrs;

/* This duplicates t_write_map, except that it updates cw_map_depth instead of
	cw_set_depth.  It is used by mupip_reorg to put bit maps on the end of
	the cw set for concurrency checking */

void mu_write_map(block_id blk,			/*  block number being written */
		  sm_uc_ptr_t old_addr,		/* address of before image of the block */
		  unsigned char *upd_addr,	/* list of blocks to be cleared in bit map */
		  int tn)
{
	cw_set_element	*cs;

	if (blk >= cs_addrs->ti->total_blks)
		GTMASSERT;

	if (!cw_map_depth)
		cw_map_depth = cw_set_depth;
 	assert(cw_map_depth < CDB_CW_SET_SIZE);
	cs = &cw_set[cw_map_depth];
	cs->mode = gds_t_writemap;
	cs->blk = blk;
	cs->old_block = old_addr;
	cs->ins_off = 0;
	cs->index = 0;
	cs->reference_cnt = 0;
	cs->upd_addr = upd_addr;
	cs->tn = tn;
	cs->level = LCL_MAP_LEVL;
	cs->jnl_freeaddr = 0;		/* reset jnl_freeaddr that previous transaction might have filled in */
	cs->write_type = GDS_WRITE_PLAIN;
 	cw_map_depth++;
}
