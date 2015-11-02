/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "jnl_write_pblk.h"
#include "min_max.h"
#include "jnl_get_checksum.h"

GBLREF 	jnl_gbls_t		jgbl;
GBLREF	boolean_t		dse_running;

void	jnl_write_pblk(sgmnt_addrs *csa, cw_set_element *cse, blk_hdr_ptr_t buffer, uint4 com_csum)
{
	struct_jrec_blk		pblk_record;
	int			tmp_jrec_size, jrec_size, zero_len;
	jnl_format_buffer 	blk_trailer;
	char			local_buff[JNL_REC_START_BNDRY + JREC_SUFFIX_SIZE];
	jrec_suffix		*suffix;
	jnl_private_control	*jpc;

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	pblk_record.prefix.jrec_type = JRT_PBLK;
	pblk_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	pblk_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	pblk_record.prefix.time = jgbl.gbl_jrec_time;
	pblk_record.blknum = cse->blk;
	/* in case we have a bad block-size, we dont want to write a PBLK larger than the GDS block size (maximum block size).
	 * in addition, check that checksum computed in t_end/tp_tend did take the adjusted bsiz into consideration.
	 */
	assert(buffer->bsiz <= csa->hdr->blk_size || dse_running);
	pblk_record.bsiz = MIN(csa->hdr->blk_size, buffer->bsiz);
	assert((pblk_record.bsiz == buffer->bsiz) ||
	       (cse->blk_checksum == jnl_get_checksum((uint4 *)buffer, NULL, pblk_record.bsiz)));
	assert(pblk_record.bsiz >= SIZEOF(blk_hdr) || dse_running);
	pblk_record.ondsk_blkver = cse->ondsk_blkver;
	tmp_jrec_size = (int)FIXED_PBLK_RECLEN + pblk_record.bsiz + JREC_SUFFIX_SIZE;
	jrec_size = ROUND_UP2(tmp_jrec_size, JNL_REC_START_BNDRY);
	zero_len = jrec_size - tmp_jrec_size;
	blk_trailer.buff = local_buff + (JNL_REC_START_BNDRY - zero_len);
	memset(blk_trailer.buff, 0, zero_len);
	blk_trailer.record_size = zero_len + JREC_SUFFIX_SIZE;
	suffix = (jrec_suffix *)&local_buff[JNL_REC_START_BNDRY];
	pblk_record.prefix.forwptr = suffix->backptr = jrec_size;
	COMPUTE_PBLK_CHECKSUM(cse->blk_checksum, &pblk_record, com_csum, pblk_record.prefix.checksum);
	suffix->suffix_code = JNL_REC_SUFFIX_CODE;
	assert(SIZEOF(uint4) == SIZEOF(jrec_suffix));
	jnl_write(jpc, JRT_PBLK, (jnl_record *)&pblk_record, buffer, &blk_trailer);
}
