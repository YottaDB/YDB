/****************************************************************
 *								*
 *	Copyright 2003, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "gtm_string.h"
#include "gtm_time.h"
#include "gtm_inet.h"
#if defined(UNIX)
#include <errno.h>
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "interlock.h"
#include "lockconst.h"
#include "aswp.h"
#elif defined(VMS)
#include <descrip.h>
#include <fab.h>
#include <iodef.h>
#include <lckdef.h>
#include <nam.h>
#include <psldef.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <xab.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "is_file_identical.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "repl_sp.h"
#include "iosp.h"	/* for SS_NORMAL */

GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;
GBLREF  jnl_process_vector      *prc_vec;

error_def(ERR_FILEIDMATCH);
error_def(ERR_JNLBADLABEL);
error_def(ERR_JNLOPNERR);
error_def(ERR_JNLRDERR);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLRECTYPE);
error_def(ERR_JNLTRANSGTR);
error_def(ERR_JNLTRANSLSS);
error_def(ERR_JNLWRERR);
error_def(ERR_JNLVSIZE);
error_def(ERR_PREMATEOF);
error_def(ERR_JNLPREVRECOV);

/* note: returns 0 on success */
uint4 jnl_file_open_common(gd_region *reg, off_jnl_t os_file_size)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t     	jb;
	jnl_file_header		header;
	struct_jrec_eof		eof_record; /* pointer is in an attempt to use make code portable */
	unsigned char		eof_rec_buffer[DISK_BLOCK_SIZE * 2];
	off_jnl_t		adjust;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	jpc->status = jpc->status2 = SS_NORMAL;
	/* Read the journal file header */
	DO_FILE_READ(jpc->channel, 0, &header, JNL_HDR_LEN, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
		return ERR_JNLRDERR;
	VMS_ONLY(adjust = header.end_of_data & (DISK_BLOCK_SIZE - 1);)
	UNIX_ONLY(adjust = 0;)
	assert(ROUND_UP2(adjust + EOF_RECLEN, DISK_BLOCK_SIZE) <= sizeof(eof_rec_buffer));
	/* Read the journal JRT_EOF at header.end_of_data offset */
	DO_FILE_READ(jpc->channel, header.end_of_data - adjust, eof_rec_buffer, EOF_RECLEN + adjust, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
		return ERR_JNLRDERR;
	if (0 != MEMCMP_LIT(header.label, JNL_LABEL_TEXT))
	{
		jpc->status = ERR_JNLBADLABEL;
		return ERR_JNLOPNERR;
	}
	if (header.prev_recov_end_of_data)
	{
		/* not possible for run time. In case it happens user must fix it */
		jpc->status = ERR_JNLPREVRECOV;
		return ERR_JNLOPNERR;
	}
	if (!is_gdid_file_identical(&FILE_ID(reg), (char *)header.data_file_name, header.data_file_name_length))
	{
		jpc->status = ERR_FILEIDMATCH;
		return ERR_JNLOPNERR;
	}
	memcpy(&eof_record, (unsigned char *)eof_rec_buffer + adjust, EOF_RECLEN);
	if (JRT_EOF != eof_record.prefix.jrec_type)
	{
		jpc->status = ERR_JNLRECTYPE;
		return ERR_JNLOPNERR;
	}
	if (eof_record.prefix.tn != csd->trans_hist.curr_tn)
	{
		if (eof_record.prefix.tn < csd->trans_hist.curr_tn)
			jpc->status = ERR_JNLTRANSLSS;
		else
			jpc->status = ERR_JNLTRANSGTR;
		return ERR_JNLOPNERR;
	}
	if (eof_record.suffix.suffix_code != JNL_REC_SUFFIX_CODE ||
		eof_record.suffix.backptr != eof_record.prefix.forwptr)
	{
		jpc->status = ERR_JNLBADRECFMT;
		return ERR_JNLOPNERR;
	}
	assert(header.eov_tn == eof_record.prefix.tn);
	header.eov_tn = eof_record.prefix.tn;
	assert(header.eov_timestamp == eof_record.prefix.time);
	header.eov_timestamp = eof_record.prefix.time;
	assert(header.eov_timestamp >= header.bov_timestamp);
	assert(((off_jnl_t)os_file_size) % JNL_REC_START_BNDRY == 0);
	if ((header.virtual_size < DIVIDE_ROUND_UP(os_file_size, DISK_BLOCK_SIZE)) ||
		(header.jnl_deq && 0 != ((header.virtual_size - header.jnl_alq) % header.jnl_deq)))
	{
		send_msg(VARLSTCNT(8) ERR_JNLVSIZE, 6, JNL_LEN_STR(csd), header.virtual_size,
			header.jnl_alq, header.jnl_deq, os_file_size);
		jpc->status = ERR_JNLVSIZE;
		return ERR_JNLOPNERR;
	}
	jb->size = csd->jnl_buffer_size * DISK_BLOCK_SIZE;
	jb->freeaddr = jb->dskaddr = UNIX_ONLY(jb->fsync_dskaddr = ) header.end_of_data;
	/* The following is to make sure that the data in jnl_buffer is aligned with the data in the
	 * disk file on an IO_BLOCK_SIZE boundary. Since we assert that jb->size is a multiple of IO_BLOCK_SIZE,
	 * alignment with respect to jb->size implies alignment with IO_BLOCK_SIZE.
	 */
	assert(0 == jb->size % IO_BLOCK_SIZE);
	jb->free = jb->dsk = header.end_of_data % jb->size;
	UNIX_ONLY(
	SET_LATCH_GLOBAL(&jb->fsync_in_prog_latch, LOCK_AVAILABLE);
	SET_LATCH_GLOBAL(&jb->io_in_prog_latch, LOCK_AVAILABLE);
	)
	VMS_ONLY(
	assert(0 == jb->now_writer);
	bci(&jb->io_in_prog);
        jb->now_writer = 0;
	assert((jb->free % DISK_BLOCK_SIZE) == adjust);
	)
	assert(JNL_WRT_START_MODULUS == DISK_BLOCK_SIZE);
	if (adjust)
	{	/* if jb->free does not start at a 512-byte boundary (which is the granularity used by VMS jnl_output_sp() for
		 * flushing to disk), copy as much pre-existing data from the journal file as necessary into the journal buffer
		 * to fill the gap so we do not lose this information in the next write to disk.
		 */
		UNIX_ONLY(assert(FALSE));
		memcpy(&jb->buff[ROUND_DOWN2(jb->free, DISK_BLOCK_SIZE)], eof_rec_buffer, adjust);
	}
	jb->filesize = header.virtual_size;
	jb->min_write_size = JNL_MIN_WRITE;
	jb->max_write_size = JNL_MAX_WRITE;
	jb->before_images = header.before_images;
	jb->epoch_tn = eof_record.prefix.tn;
	csd->jnl_checksum = header.checksum;
	LOG2_OF_INTEGER(header.alignsize, jb->log2_of_alignsize);
	assert(header.autoswitchlimit == csd->autoswitchlimit);
	assert(header.jnl_alq == csd->jnl_alq);
	assert(header.jnl_deq == csd->jnl_deq);
	assert(csd->autoswitchlimit >= csd->jnl_alq);
	assert(ALIGNED_ROUND_UP(csd->autoswitchlimit, csd->jnl_alq, csd->jnl_deq) == csd->autoswitchlimit);
	assert(csd->autoswitchlimit);
	JNL_WHOLE_TIME(prc_vec->jpv_time);
	jb->epoch_interval = header.epoch_interval;
	jb->next_epoch_time = MID_TIME(prc_vec->jpv_time) + jb->epoch_interval;
	memcpy(&header.who_opened, prc_vec, sizeof(jnl_process_vector));
	header.crash = TRUE;	/* in case this processes is crashed, this will remain TRUE */
	if (REPL_ENABLED(csd) && pool_init)
		header.update_disabled = jnlpool_ctl->upd_disabled;
	DO_FILE_WRITE(jpc->channel, 0, &header, JNL_HDR_LEN, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
		return ERR_JNLWRERR;
	jb->end_of_data = 0;
	jb->eov_tn = 0;
	jb->eov_timestamp = 0;
	jb->end_seqno = 0;
	return 0;
}
