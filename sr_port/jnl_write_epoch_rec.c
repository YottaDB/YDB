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
#include "gtmio.h"
#include "iosp.h"
#include "jnl_get_checksum.h"

GBLREF 	jnl_gbls_t		jgbl;
GBLREF  jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	seq_num			seq_num_zero;

void	jnl_write_epoch_rec(sgmnt_addrs *csa)
{
	struct_jrec_epoch	epoch_record;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	jnl_file_header		header;
	sgmnt_data_ptr_t	csd;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif

	error_def		(ERR_PREMATEOF);

	assert(csa->now_crit);
	jpc = csa->jnl;
	assert(0 != jpc->pini_addr);
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	csd = csa->hdr;
	epoch_record.prefix.jrec_type = JRT_EPOCH;
	epoch_record.prefix.forwptr = epoch_record.suffix.backptr = EPOCH_RECLEN;
	epoch_record.blks_to_upgrd = csd->blks_to_upgrd;
	epoch_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	/* in case jpc->pini_addr turns out to be zero (not clear how), we use the pini_addr field of the
	 * first PINI journal record in the journal file which is nothing but JNL_HDR_LEN.
	 */
	epoch_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
	jb = jpc->jnl_buff;
	jb->epoch_tn = epoch_record.prefix.tn = csa->ti->curr_tn;
	/* At this point jgbl.gbl_jrec_time should be set by the caller */
	assert(jgbl.gbl_jrec_time);
	epoch_record.prefix.time = jgbl.gbl_jrec_time;
	/* we need to write epochs if jgbl.forw_phase_recovery so future recovers will have a closer turnaround point */
	jb->next_epoch_time =  epoch_record.prefix.time + jb->epoch_interval;
	epoch_record.prefix.checksum = INIT_CHECKSUM_SEED;
	ASSERT_JNL_SEQNO_FILEHDR_JNLPOOL(csd, jnlpool_ctl);	/* debug-only sanity check between seqno of filehdr and jnlpool */
	if (jgbl.forw_phase_recovery)
		/* As the file header is not flushed too often and recover/rollback doesn't update reg_seqno */
		QWASSIGN(epoch_record.jnl_seqno, jgbl.mur_jrec_seqno);
	else if (REPL_ALLOWED(csd))
		QWASSIGN(epoch_record.jnl_seqno, csd->reg_seqno);	/* Note we cannot use jnlpool_ctl->jnl_seqno since
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
	jnl_write(jpc, JRT_EPOCH, (jnl_record *)&epoch_record, NULL, NULL);
}
