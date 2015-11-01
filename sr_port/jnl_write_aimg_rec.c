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

#include "gtm_time.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write_aimg_rec.h"
#include "jnl_write.h"

void	jnl_write_aimg_rec(sgmnt_addrs *csa, block_id block, blk_hdr_ptr_t buffer)
{
	struct_jrec_after_image	aimg_record;

	aimg_record.pini_addr = csa->jnl->pini_addr;
	JNL_SHORT_TIME(aimg_record.short_time);
	aimg_record.tn = csa->ti->curr_tn;
	aimg_record.blknum = block;
	aimg_record.bsiz = buffer->bsiz;

	jnl_write(csa->jnl, JRT_AIMG, (jrec_union *)&aimg_record, buffer, NULL);
}
