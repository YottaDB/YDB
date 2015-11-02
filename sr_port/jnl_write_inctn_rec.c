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

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif
#include "gtm_inet.h"

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
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl_get_checksum.h"

GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	inctn_opcode_t		inctn_opcode;
GBLREF	inctn_detail_t		inctn_detail;			/* holds detail to fill in to inctn jnl record */

void	jnl_write_inctn_rec(sgmnt_addrs	*csa)
{
	struct_jrec_inctn	inctn_record;
	jnl_private_control	*jpc;
	DEBUG_ONLY(int		inctn_detail_size;)

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	inctn_record.prefix.jrec_type = JRT_INCTN;
	inctn_record.prefix.forwptr = INCTN_RECLEN;
	assert(&inctn_detail.blknum_struct.suffix == &inctn_detail.blks2upgrd_struct.suffix);
	DEBUG_ONLY(inctn_detail_size = OFFSETOF(inctn_detail_blknum_t, suffix) + SIZEOF(inctn_detail.blknum_struct.suffix);)
	assert(0 == (inctn_detail_size % JNL_REC_START_BNDRY));
	assert(SIZEOF(inctn_detail) == inctn_detail_size);
	inctn_detail.blknum_struct.suffix.backptr = INCTN_RECLEN;
	inctn_detail.blknum_struct.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	inctn_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	inctn_record.prefix.time = jgbl.gbl_jrec_time;
	inctn_record.prefix.tn = csa->ti->curr_tn;
	inctn_record.prefix.checksum = INIT_CHECKSUM_SEED;
	assert((inctn_opcode_total > inctn_opcode) && (inctn_invalid_op < inctn_opcode));
	/* Assert that the maximum inctn opcode # will fit in the "opcode" field in the inctn jnl record.
	 * But before that, assert opcode is at same offset in all the individual inctn_detail_* structure types.
	 */
	assert(&inctn_detail.blknum_struct.opcode == &inctn_detail.blks2upgrd_struct.opcode);
	assert(inctn_opcode_total < (1 << (8 * SIZEOF(inctn_detail.blknum_struct.opcode))));
	inctn_detail.blknum_struct.opcode = inctn_opcode;	/* fill in opcode from the global variable */
	/* Instead of having a multi-line switch statement that copies exactly those fields which are necessary, we
	 * copy the entire structure (16 bytes at this point). Pipeline breaks are considered more costly than a few
	 * unnecessary memory-to-memory copies.
	 */
	inctn_detail.blknum_struct.filler_uint4 = 0;
	inctn_detail.blknum_struct.filler_short = 0;
	inctn_record.detail = inctn_detail;
	inctn_record.prefix.checksum = compute_checksum(INIT_CHECKSUM_SEED, (uint4 *)&inctn_record, SIZEOF(struct_jrec_inctn));
	jnl_write(jpc, JRT_INCTN, (jnl_record *)&inctn_record, NULL, NULL);
}
