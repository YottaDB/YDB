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

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
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
LITREF	int	jrt_update[JRT_RECTYPES];
#endif

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	short			dollar_tlevel;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	seq_num			seq_num_zero;

/* This called for TP and non-TP, but not for ZTP */
void	jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *jfb)
{
	struct_jrec_upd	*jrec;

	assert(0 != csa->jnl->pini_addr);
	assert(csa->now_crit);
	assert(IS_SET_KILL_ZKILL(jfb->rectype));
	assert(!IS_ZTP(jfb->rectype));
	jrec = (struct_jrec_upd *)jfb->buff;
	jrec->prefix.pini_addr = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	jrec->prefix.tn = csa->ti->curr_tn;
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	jrec->prefix.time = jgbl.gbl_jrec_time;
	if (jgbl.forw_phase_recovery)
	{
		QWASSIGN(jrec->token_seq, jgbl.mur_jrec_token_seq);
	} else
	{	/* t_end and tp_tend already has set token or jnl_seqno into jnl_fence_ctl.token */
		QWASSIGN(jrec->token_seq.token, jnl_fence_ctl.token);
	}
	jnl_write(csa->jnl, jfb->rectype, (jnl_record *)jrec, NULL, jfb);
}
