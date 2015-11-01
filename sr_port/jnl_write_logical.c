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

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "iosp.h"
#include "jnl_write.h"

GBLDEF	uint4			rec_seqno;		/* if not for TCOM record in tp_tend, can be made static */

GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	short			dollar_tlevel;
GBLREF	jnlpool_ctl_ptr_t	temp_jnlpool_ctl;
GBLREF	uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */
GBLREF  uint4                   zts_jrec_time;
GBLREF	fixed_jrec_tp_kill_set 	mur_jrec_fixed_field;
GBLREF 	boolean_t		is_updproc;
GBLREF 	boolean_t		copy_jnl_record;

void	jnl_write_logical(sgmnt_addrs *csa, jnl_format_buffer *hdr_buffer)
{
	fixed_jrec_tp_kill_set	*jrec;

	jrec = (fixed_jrec_tp_kill_set *)(hdr_buffer->buff + JREC_PREFIX_SIZE);
	jrec->pini_addr = csa->jnl->pini_addr;
	assert(JRT_INCTN != hdr_buffer->rectype && JRT_AIMG != hdr_buffer->rectype);
	if (jnl_fence_ctl.level == 0  &&  dollar_tlevel == 0)
		rec_seqno = 0;
	else
	{
		assert(jnl_fence_ctl.region_count >= 0);
		if (jnl_fence_ctl.region_count == 0 && !copy_jnl_record)
		{
			rec_seqno = 0;
			QWASSIGN2DW(jnl_fence_ctl.token, csa->jnl->regnum, csa->ti->curr_tn);
			/* Earlier, we used to write the same time in all journal records for the same ZTransaction.
			 * But that is now changed so that each update in a ZTransaction has its current timestamp.
			 * The global zts_jrec_time stores the time when the ZTSTART record was written, to
			 * facilitate copying this over into the ZTCOM jnl record when the ZTCOMMIT is encountered.
			 */
			if (!dollar_tlevel)
				zts_jrec_time = gbl_jrec_time;
			++jnl_fence_ctl.region_count;
		}
		QWASSIGN(jrec->token, jnl_fence_ctl.token);
	}
	jrec->recov_short_time = (!is_updproc ? gbl_jrec_time : mur_jrec_fixed_field.recov_short_time);
	if (!copy_jnl_record)
	{
		jrec->short_time = gbl_jrec_time;
		jrec->rec_seqno = rec_seqno;
		QWASSIGN(jrec->jnl_seqno, temp_jnlpool_ctl->jnl_seqno);
	} else
	{
		jrec->short_time = mur_jrec_fixed_field.short_time;
		jrec->rec_seqno = mur_jrec_fixed_field.rec_seqno;
		QWASSIGN(jrec->jnl_seqno, mur_jrec_fixed_field.jnl_seqno);
	}
	jrec->tn = csa->ti->curr_tn;
	jnl_write(csa->jnl, hdr_buffer->rectype, NULL, NULL, hdr_buffer);
	rec_seqno++;	/* not needed for copy_jnl_record case, but speed up GT.M normal jnl path */
}
