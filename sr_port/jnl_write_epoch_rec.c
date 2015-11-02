/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
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

error_def	(ERR_PREMATEOF);

void	jnl_write_epoch_rec(sgmnt_addrs *csa)
{
	struct_jrec_epoch	epoch_record;
	jnl_buffer_ptr_t	jb;
	jnl_private_control	*jpc;
	jnl_file_header		*header;
	unsigned char		hdr_base[REAL_JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	sgmnt_data_ptr_t	csd;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif
	uint4			jnl_fs_block_size, read_write_size;

	assert(csa->now_crit);
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	assert((csa->ti->early_tn == csa->ti->curr_tn) || (csa->ti->early_tn == csa->ti->curr_tn + 1));
	assert(0 != jpc->pini_addr);
	csd = csa->hdr;
	epoch_record.prefix.jrec_type = JRT_EPOCH;
	epoch_record.prefix.forwptr = epoch_record.suffix.backptr = EPOCH_RECLEN;
	epoch_record.blks_to_upgrd = csd->blks_to_upgrd;
	epoch_record.total_blks    = csd->trans_hist.total_blks;
	epoch_record.free_blocks   = csd->trans_hist.free_blocks;
	epoch_record.suffix.suffix_code = JNL_REC_SUFFIX_CODE;
	/* in case jpc->pini_addr turns out to be zero (not clear how), we use the pini_addr field of the
	 * first PINI journal record in the journal file which is nothing but JNL_HDR_LEN.
	 */
	epoch_record.prefix.pini_addr = (0 == jpc->pini_addr) ? JNL_HDR_LEN : jpc->pini_addr;
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
		jnl_fs_block_size = jb->fs_block_size;
		header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_base, jnl_fs_block_size));
		read_write_size = ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size);
		assert((unsigned char *)header + read_write_size <= ARRAYTOP(hdr_base));
		DO_FILE_READ(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
		assert(SS_NORMAL != jpc->status || SS_NORMAL == jpc->status2);
		if (SS_NORMAL == jpc->status)
		{
			header->end_of_data = jb->end_of_data;
			csa->hdr->jnl_eovtn = header->eov_tn;
			header->eov_tn = jb->eov_tn;
			header->eov_timestamp = jb->eov_timestamp;
			header->end_seqno = jb->end_seqno;
			DO_FILE_WRITE(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
			/* for abnormal status do not do anything. journal file header will have previous end_of_data */
		}
	}
	jb->end_of_data = jb->freeaddr;
	jb->eov_tn = csa->ti->curr_tn;
	jb->eov_timestamp = jgbl.gbl_jrec_time;
	jb->end_seqno = epoch_record.jnl_seqno;
	jnl_write(jpc, JRT_EPOCH, (jnl_record *)&epoch_record, NULL, NULL);
}
