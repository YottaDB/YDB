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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write_pblk.h"
#include "jnl_write.h"

GBLREF uint4		gbl_jrec_time;	/* see comment in gbldefs.c for usage */
GBLREF uint4		cur_logirec_short_time;	/* see comment in gbldefs.c for usage */
GBLREF boolean_t	forw_phase_recovery;

void	jnl_write_pblk(sgmnt_addrs *csa, block_id block, blk_hdr_ptr_t buffer)
{
	struct_jrec_pblk	pblk_record;

	pblk_record.pini_addr = csa->jnl->pini_addr;
	pblk_record.short_time = (forw_phase_recovery ? cur_logirec_short_time : gbl_jrec_time);
	pblk_record.blknum = block;
	pblk_record.bsiz = buffer->bsiz;
	jnl_write(csa->jnl, JRT_PBLK, (jrec_union *)&pblk_record, buffer, NULL);
}
