/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
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
#ifdef DEBUG
#include "jnl_typedef.h"
LITREF	int	jrt_update[JRT_RECTYPES];	/* For IS_SET_KILL_ZKILL macro */
#endif

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;

void	jnl_write_ztp_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb)
{
	struct_jrec_ztp_upd	*jrec;

	assert(0 != csa->jnl->pini_addr);
	assert(csa->now_crit);
	assert(IS_SET_KILL_ZKILL(jfb->rectype));
	assert(IS_ZTP(jfb->rectype));
	jrec = (struct_jrec_ztp_upd *)jfb->buff;
	jrec->prefix.pini_addr = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	jrec->prefix.tn = csa->ti->curr_tn;
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	jrec->prefix.time = jgbl.gbl_jrec_time;
	jrec->prefix.checksum = jfb->checksum;
	if (jgbl.forw_phase_recovery)
	{
		QWASSIGN(jrec->jnl_seqno, jgbl.mur_jrec_seqno);
		QWASSIGN(jrec->token, jgbl.mur_jrec_token_seq.token);
	} else
	{
		if (REPL_ENABLED(csa))
			QWASSIGN(jrec->jnl_seqno, temp_jnlpool_ctl->jnl_seqno);
		else
			QWASSIGN(jrec->jnl_seqno, seq_num_zero);
		if (QWEQ(jnl_fence_ctl.token, seq_num_zero))
		{	/* generate token once after op_ztstart and use for all its mini-transactions
			 * jnl_fence_ctl.token is set to seq_num_zero in op_ztstart */
			if (REPL_ENABLED(csa))
				QWASSIGN(jnl_fence_ctl.token, temp_jnlpool_ctl->jnl_seqno);
			else
			{
				TOKEN_SET(&jnl_fence_ctl.token, csa->ti->curr_tn, csa->regnum);
			}
		}
		QWASSIGN(jrec->token, jnl_fence_ctl.token);
	}
	jnl_write(csa->jnl, jfb->rectype, (jnl_record *)jrec, NULL, jfb);
}
