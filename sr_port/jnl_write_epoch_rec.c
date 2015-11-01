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

#if defined(UNIX)
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#elif defined(VMS)
#include <descrip.h> /* Required for gtmsource.h */
#include <rms.h>
#include <iodef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>

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
#include "gtmio.h"
#include "iosp.h"

GBLREF 	jnl_gbls_t		jgbl;
GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	seq_num			seq_num_zero;

void	jnl_write_epoch_rec(sgmnt_addrs *csa)
{
	struct_jrec_epoch	epoch_record;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	jnl_file_header		header;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif
	error_def		(ERR_PREMATEOF);

	assert(csa->now_crit);
	assert(0 != csa->jnl->pini_addr);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	epoch_record.prefix.jrec_type = JRT_EPOCH;
	epoch_record.prefix.forwptr = epoch_record.suffix.backptr = EPOCH_RECLEN;
	epoch_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	/* in case csa->jnl->pini_addr turns out to be zero (not clear how), we use the pini_addr field of the
	 * first PINI journal record in the journal file which is nothing but JNL_HDR_LEN.
	 */
	epoch_record.prefix.pini_addr = (0 == csa->jnl->pini_addr) ? JNL_HDR_LEN : csa->jnl->pini_addr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	jb->epoch_tn = epoch_record.prefix.tn = csa->ti->curr_tn;
	assert(jgbl.gbl_jrec_time);
	if (!jgbl.gbl_jrec_time)
	{	/* no idea how this is possible, but just to be safe */
		JNL_SHORT_TIME(jgbl.gbl_jrec_time);
	}
	epoch_record.prefix.time = jgbl.gbl_jrec_time;
	/* we need to write epochs if jgbl.forw_phase_recovery so future recovers will have a closer turnaround point */
	jb->next_epoch_time =  epoch_record.prefix.time + jb->epoch_interval;
	assert(NULL == jnlpool_ctl  ||  QWLE(csa->hdr->reg_seqno, jnlpool_ctl->jnl_seqno));
	if (jgbl.forw_phase_recovery)
		/* As the file header is not flushed too often and recover/rollback doesn't update reg_seqno */
		QWASSIGN(epoch_record.jnl_seqno, jgbl.mur_jrec_seqno);
	else if (REPL_ENABLED(csa->hdr))
		QWASSIGN(epoch_record.jnl_seqno, csa->hdr->reg_seqno);	/* Note we cannot use jnlpool_ctl->jnl_seqno since
									 * we might not presently hold the journal pool lock */
	else
		QWASSIGN(epoch_record.jnl_seqno, seq_num_zero);
	if (jb->end_of_data)
	{
		DO_FILE_READ(jpc->channel, 0, &header, JNL_HDR_LEN, jpc->status, jpc->status2);
		assert(SS_NORMAL != jpc->status || SS_NORMAL == jpc->status2);
		if (SS_NORMAL == jpc->status)
		{
			header.end_of_data = jb->end_of_data;
			header.eov_tn = jb->eov_tn;
			header.eov_timestamp = jb->eov_timestamp;
			header.end_seqno = jb->end_seqno;
			DO_FILE_WRITE(jpc->channel, 0, &header, JNL_HDR_LEN, jpc->status, jpc->status2);
			/* for abnormal status do not do anything. journal file header will have previous end_of_data */
		}
	}
	jb->end_of_data = jb->freeaddr;
	jb->eov_tn = csa->ti->curr_tn;
	jb->eov_timestamp = jgbl.gbl_jrec_time;
	jb->end_seqno = epoch_record.jnl_seqno;
	jnl_write(csa->jnl, JRT_EPOCH, (jnl_record *)&epoch_record, NULL, NULL);
}
