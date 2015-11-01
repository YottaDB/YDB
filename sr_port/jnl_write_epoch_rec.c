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

#include "gtm_time.h"
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
#include "jnl_write.h"

GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	seq_num			seq_num_zero;
GBLREF  uint4			gbl_jrec_time;	/* see comment in gbldefs.c for usage */

void	jnl_write_epoch_rec(sgmnt_addrs *csa)
{
	struct_jrec_epoch	epoch_record;
	jnl_buffer_ptr_t	jb;

	epoch_record.pini_addr = csa->jnl->pini_addr;
	if (csa->ti->early_tn != csa->ti->curr_tn)
	{	/* means we have already set gbl_jrec_time in either t_end() or tp_tend() by invoking the JNL_SHORT_TIME macro.
		 * so use that instead of invoking the system time() call.
		 */
		assert(csa->ti->early_tn == csa->ti->curr_tn + 1);
		epoch_record.short_time = gbl_jrec_time;
	} else
	{
		JNL_SHORT_TIME(epoch_record.short_time);
	}
	epoch_record.tn = csa->ti->curr_tn;
	jb = csa->jnl->jnl_buff;
	jb->epoch_tn = csa->ti->curr_tn;
	jb->next_epoch_time = epoch_record.short_time + jb->epoch_interval;
	assert(NULL == jnlpool_ctl  ||  QWLE(csa->hdr->reg_seqno, jnlpool_ctl->jnl_seqno));
	if (REPL_ENABLED(csa->hdr))
		QWASSIGN(epoch_record.jnl_seqno, csa->hdr->reg_seqno);
	else
		QWASSIGN(epoch_record.jnl_seqno, seq_num_zero);

	jnl_write(csa->jnl, JRT_EPOCH, (jrec_union *)&epoch_record, NULL, NULL);
}
