/****************************************************************
 *								*
 *	Copyright 2003, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_inet.h"
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

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
#include "iosp.h"
#include "jnl_get_checksum.h"
#ifdef DEBUG
#include "jnl_typedef.h"
#endif

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;
GBLREF	trans_num		local_tn;	/* transaction number for THIS PROCESS */
GBLREF	uint4			process_id;

void	jnl_write_ztp_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb, uint4 com_csum)
{
	struct_jrec_upd		*jrec;
	volatile seq_num	temp_seqno;
	GTMCRYPT_ONLY(
		struct_jrec_upd	*jrec_alt;
	)
	jnl_private_control	*jpc;

	/* If REPL_WAS_ENABLED(csa) is TRUE, then we would not have gone through the code that initializes
	 * jgbl.gbl_jrec_time or jpc->pini_addr. But in this case, we are not writing the journal record
	 * to the journal buffer or journal file but write it only to the journal pool from where it gets
	 * sent across to the update process that does not care about these fields so it is ok to leave them as is.
	 */
	jpc = csa->jnl;
	assert((0 != jpc->pini_addr) || REPL_WAS_ENABLED(csa));
	assert(jgbl.gbl_jrec_time || REPL_WAS_ENABLED(csa));
	assert(csa->now_crit);
	assert(IS_SET_KILL_ZKILL_ZTRIG(jfb->rectype));
	assert(IS_ZTP(jfb->rectype));
	jrec = (struct_jrec_upd *)jfb->buff;
	jrec->prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	jrec->prefix.tn = csa->ti->curr_tn;
	jrec->prefix.time = jgbl.gbl_jrec_time;
	temp_seqno = temp_jnlpool_ctl->jnl_seqno;
	if (QWEQ(jnl_fence_ctl.token, seq_num_zero))
	{	/* generate token once after op_ztstart and use for all its mini-transactions
		 * jnl_fence_ctl.token is set to seq_num_zero in op_ztstart */
		if (REPL_ALLOWED(csa))
			QWASSIGN(jnl_fence_ctl.token, temp_seqno);
		else
		{
			TOKEN_SET(&jnl_fence_ctl.token, local_tn, process_id);
		}
	}
	assert(0 != jnl_fence_ctl.token);
	jrec->token_seq.token = jnl_fence_ctl.token;
	jrec->strm_seqno = 0;	/* strm_seqno is only for replication & ZTCOM does not work with replic */
	COMPUTE_LOGICAL_REC_CHECKSUM(jfb->checksum, jrec, com_csum, jrec->prefix.checksum);
	GTMCRYPT_ONLY(assert(!REPL_ALLOWED(csa));)
	JNL_WRITE_APPROPRIATE(csa, jpc, jfb->rectype, (jnl_record *)jrec, NULL, jfb);
}
