/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>

#include "gtm_string.h"
#include "gtm_time.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_write.h"
#include "jnl_write_pblk.h"

GBLREF 	jnl_gbls_t		jgbl;

void	jnl_write_pblk(sgmnt_addrs *csa, block_id block, blk_hdr_ptr_t buffer)
{
	struct_jrec_blk		pblk_record;
	int			tmp_jrec_size, jrec_size, zero_len;
	jnl_format_buffer 	blk_trailer;
	char			local_buff[JNL_REC_START_BNDRY + JREC_SUFFIX_SIZE];
	jrec_suffix		*suffix;

	assert(0 != csa->jnl->pini_addr);
	assert(csa->now_crit);
	pblk_record.prefix.jrec_type = JRT_PBLK;
	pblk_record.prefix.pini_addr = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	pblk_record.prefix.tn = csa->ti->curr_tn;
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	pblk_record.prefix.time = jgbl.gbl_jrec_time;
	pblk_record.blknum = block;
	pblk_record.bsiz = buffer->bsiz;
	tmp_jrec_size = FIXED_PBLK_RECLEN + pblk_record.bsiz + JREC_SUFFIX_SIZE;
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	zero_len = jrec_size - tmp_jrec_size;
	blk_trailer.buff = local_buff + (JNL_REC_START_BNDRY - zero_len);
	memset(blk_trailer.buff, 0, zero_len);
	blk_trailer.record_size = zero_len + JREC_SUFFIX_SIZE;
	suffix = (jrec_suffix *)&local_buff[JNL_REC_START_BNDRY];
	pblk_record.prefix.forwptr = suffix->backptr = jrec_size;
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;
	assert(sizeof(uint4) == sizeof(jrec_suffix));
	jnl_write(csa->jnl, JRT_PBLK, (jnl_record *)&pblk_record, buffer, &blk_trailer);
}
