/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	inctn_record.prefix.jrec_type = JRT_INCTN;
	inctn_record.prefix.forwptr = inctn_record.suffix.backptr = INCTN_RECLEN;
	inctn_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	inctn_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	inctn_record.prefix.time = jgbl.gbl_jrec_time;
	inctn_record.prefix.tn = csa->ti->curr_tn;
	inctn_record.prefix.checksum = INIT_CHECKSUM_SEED;
	assert(inctn_opcode_total > inctn_opcode && inctn_invalid_op < inctn_opcode);
	inctn_record.opcode = inctn_opcode;
	switch(inctn_opcode)
	{
		case inctn_bmp_mark_free_gtm:
		case inctn_bmp_mark_free_mu_reorg:
		case inctn_blkupgrd:
		case inctn_blkdwngrd:
		case inctn_blkupgrd_fmtchng:
		case inctn_blkdwngrd_fmtchng:
		case inctn_blkmarkfree:
			inctn_record.detail.blknum = inctn_detail.blknum;
			break;
		case inctn_gdsfilext_gtm:
		case inctn_gdsfilext_mu_reorg:
		case inctn_db_format_change:
			inctn_record.detail.blks_to_upgrd_delta = inctn_detail.blks_to_upgrd_delta;
			break;
		default:
			break;
	}
	jnl_write(jpc, JRT_INCTN, (jnl_record *)&inctn_record, NULL, NULL);
}
