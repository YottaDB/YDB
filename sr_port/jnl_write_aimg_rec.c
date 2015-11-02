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
#include "gdscc.h"
#include "jnl.h"
#include "jnl_write.h"
#include "jnl_write_aimg_rec.h"
#include "jnl_get_checksum.h"
#include "min_max.h"

GBLREF 	jnl_gbls_t		jgbl;

void jnl_write_aimg_rec(sgmnt_addrs *csa, cw_set_element *cse)
{
	struct_jrec_blk		aimg_record;
	int			tmp_jrec_size, jrec_size, zero_len;
	jnl_format_buffer 	blk_trailer;	/* partial record after the aimg block */
	char			local_buff[JNL_REC_START_BNDRY + JREC_SUFFIX_SIZE];
	jrec_suffix		*suffix;
	blk_hdr_ptr_t		buffer;
	jnl_private_control	*jpc;

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	aimg_record.prefix.jrec_type = JRT_AIMG;
	aimg_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	aimg_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	aimg_record.prefix.time = jgbl.gbl_jrec_time;
	aimg_record.prefix.checksum = INIT_CHECKSUM_SEED;
	aimg_record.blknum = cse->blk;
	/* in case we have a bad block-size, we dont want to write an AIMG larger than the GDS block size (maximum block size) */
	buffer = (blk_hdr_ptr_t)cse->new_buff;
	assert(buffer->bsiz <= csa->hdr->blk_size);
	assert(buffer->bsiz >= sizeof(blk_hdr));
	aimg_record.bsiz = MIN(csa->hdr->blk_size, buffer->bsiz);
	aimg_record.ondsk_blkver = cse->ondsk_blkver;
	tmp_jrec_size = FIXED_AIMG_RECLEN + aimg_record.bsiz + JREC_SUFFIX_SIZE;
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	zero_len = jrec_size - tmp_jrec_size;
	blk_trailer.buff = local_buff + (JNL_REC_START_BNDRY - zero_len);
	memset(blk_trailer.buff, 0, zero_len);
	blk_trailer.record_size = zero_len + JREC_SUFFIX_SIZE;
	suffix = (jrec_suffix *)&local_buff[JNL_REC_START_BNDRY];
	aimg_record.prefix.forwptr = suffix->backptr = jrec_size;
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;
	assert(sizeof(uint4) == sizeof(jrec_suffix));
	jnl_write(jpc, JRT_AIMG, (jnl_record *)&aimg_record, buffer, &blk_trailer);
}
