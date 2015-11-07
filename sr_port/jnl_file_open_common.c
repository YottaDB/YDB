/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#include "get_fs_block_size.h"
#include "anticipatory_freeze.h"

GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;
GBLREF  jnl_process_vector      *prc_vec;
GBLREF	jnl_gbls_t		jgbl;

error_def(ERR_FILEIDMATCH);
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
#ifdef GTM_CRYPT
error_def(ERR_CRYPTJNLWRONGHASH);
#endif

/* note: returns 0 on success */
uint4 jnl_file_open_common(gd_region *reg, off_jnl_t os_file_size)
{
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t     	jb;
	jnl_file_header		*header;
	unsigned char		hdr_buff[REAL_JNL_HDR_LEN + MAX_IO_BLOCK_SIZE];
	struct_jrec_eof		eof_record; /* pointer is in an attempt to use make code portable */
	unsigned char		*eof_rec_buffer;
	unsigned char		eof_rec[(DISK_BLOCK_SIZE * 2) + MAX_IO_BLOCK_SIZE];
	off_jnl_t		adjust;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif
	uint4			jnl_fs_block_size, read_write_size, read_size;
	gtm_uint64_t		header_virtual_size;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	jpc->status = jpc->status2 = SS_NORMAL;
	jnl_fs_block_size = get_fs_block_size(jpc->channel);
	/* check that the filesystem block size is a power of 2 as we do a lot of calculations below assuming this is the case */
	assert(!(jnl_fs_block_size & (jnl_fs_block_size - 1)));
	header = (jnl_file_header *)(ROUND_UP2((uintszofptr_t)hdr_buff, jnl_fs_block_size));
	eof_rec_buffer = (unsigned char *)(ROUND_UP2((uintszofptr_t)eof_rec, jnl_fs_block_size));
	/* Read the journal file header */
	read_write_size = ROUND_UP2(REAL_JNL_HDR_LEN, jnl_fs_block_size);
	assert((unsigned char *)header + read_write_size <= ARRAYTOP(hdr_buff));
	DO_FILE_READ(jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
	{	/* A PREMATEOF error is possible in Unix if a V54001 version is trying to open a pre-V54001 journal file
		 * This is because starting V54001, the journal file size is always maintained as a multiple of the underlying
		 * filesystem block size. And so in case of a previous version created journal file, it is possible the
		 * entire unaligned journal file size is lesser than the aligned journal file header size.
		 */
		UNIX_ONLY(assert(ERR_PREMATEOF == jpc->status);)
		VMS_ONLY(assert(FALSE);)
		return ERR_JNLRDERR;
	}
	/* Check if the header format matches our format. Cannot access any fields inside header unless this matches */
	CHECK_JNL_FILE_IS_USABLE(header, jpc->status, FALSE, 0, NULL);	/* FALSE => NO gtm_putmsg even if errors */
	if (SS_NORMAL != jpc->status)
		return ERR_JNLOPNERR;
	adjust = header->end_of_data & (jnl_fs_block_size - 1);
	/* Read the journal JRT_EOF at header->end_of_data offset.
	 * Make sure the buffer being read to is big enough and that as part of the read,
	 * we never touch touch the journal file header territory.
	 */
	read_size = ROUND_UP2((EOF_RECLEN + adjust), jnl_fs_block_size);
	assert(eof_rec_buffer + read_size <= ARRAYTOP(eof_rec));
	assert(header->end_of_data - adjust >= JNL_HDR_LEN);
	DO_FILE_READ(jpc->channel, header->end_of_data - adjust, eof_rec_buffer, read_size, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
	{
		return ERR_JNLRDERR;
	}
	if (header->prev_recov_end_of_data)
	{
		/* not possible for run time. In case it happens user must fix it */
		jpc->status = ERR_JNLPREVRECOV;
		return ERR_JNLOPNERR;
	}
	if (!is_gdid_file_identical(&FILE_ID(reg), (char *)header->data_file_name, header->data_file_name_length))
	{
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), ERR_FILEIDMATCH);
		assert(FALSE);	/* we dont expect the rts_error in the line above to return */
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
	GTMCRYPT_ONLY(
		if (memcmp(header->encryption_hash, csd->encryption_hash, GTMCRYPT_HASH_LEN))
		{
			send_msg_csa(CSA_ARG(csa) VARLSTCNT(6) ERR_CRYPTJNLWRONGHASH, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
			jpc->status = ERR_CRYPTJNLWRONGHASH;
			return ERR_JNLOPNERR;
		}
	)
	assert(header->eov_tn == eof_record.prefix.tn);
	header->eov_tn = eof_record.prefix.tn;
	assert(header->eov_timestamp == eof_record.prefix.time);
	header->eov_timestamp = eof_record.prefix.time;
	assert(header->eov_timestamp >= header->bov_timestamp);
	assert(((off_jnl_t)os_file_size) % JNL_REC_START_BNDRY == 0);
	assert(((off_jnl_t)os_file_size) % DISK_BLOCK_SIZE == 0);
	assert(((off_jnl_t)os_file_size) % jnl_fs_block_size == 0);
	header_virtual_size = header->virtual_size;	/* saving in 8-byte int to avoid overflow below */
	if ((ROUND_UP2((header_virtual_size * DISK_BLOCK_SIZE), jnl_fs_block_size) < os_file_size)
		|| (header->jnl_deq && 0 != ((header_virtual_size - header->jnl_alq) % header->jnl_deq)))
	{
		send_msg_csa(CSA_ARG(csa) VARLSTCNT(8) ERR_JNLVSIZE, 6, JNL_LEN_STR(csd), header->virtual_size,
			 header->jnl_alq, header->jnl_deq, os_file_size, jnl_fs_block_size);
		jpc->status = ERR_JNLVSIZE;
		return ERR_JNLOPNERR;
	}
	/* For performance reasons (to be able to do aligned writes to the journal file), we need to ensure the journal buffer
	 * address is filesystem-block-size aligned in Unix. Although this is needed only in case of sync_io/direct-io, we ensure
	 * this alignment unconditionally in Unix. jb->buff_off is the number of bytes to go past before getting an aligned buffer.
	 * For VMS, this performance enhancement is currently not done and can be revisited later.
	 */
	UNIX_ONLY(jb->buff_off = (uintszofptr_t)ROUND_UP2((uintszofptr_t)&jb->buff[0], jnl_fs_block_size)
					- (uintszofptr_t)&jb->buff[0];)
	VMS_ONLY(jb->buff_off = 0;)
	jb->size = ROUND_DOWN2(csd->jnl_buffer_size * DISK_BLOCK_SIZE - jb->buff_off, jnl_fs_block_size);
	/* Assert that journal buffer does NOT spill past the allocated journal buffer size in shared memory */
	assert((sm_uc_ptr_t)&jb->buff[jb->buff_off + jb->size] < ((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE(csd)
											+ JNL_SHARE_SIZE(csd)));
	assert((sm_uc_ptr_t)jb == ((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE(csd) + JNL_NAME_EXP_SIZE));
	jb->freeaddr = jb->dskaddr = UNIX_ONLY(jb->fsync_dskaddr = ) header->end_of_data;
	jb->fs_block_size = jnl_fs_block_size;
	/* The following is to make sure that the data in jnl_buffer is aligned with the data in the
	 * disk file on an jnl_fs_block_size boundary. Since we assert that jb->size is a multiple of jnl_fs_block_size,
	 * alignment with respect to jb->size implies alignment with jnl_fs_block_size.
	 */
	assert(0 == (jb->size % jnl_fs_block_size));
	jb->free = jb->dsk = header->end_of_data % jb->size;
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
	assert(0 == (jnl_fs_block_size % DISK_BLOCK_SIZE));
	if (adjust)
	{	/* if jb->free does not start at a filesystem-block-size aligned boundary (which is the alignment granularity used
		 * by "jnl_output_sp" for flushing to disk), copy as much pre-existing data from the journal file as necessary into
		 * the journal buffer to fill the gap so we do not lose this information in the next write to disk.
		 */
		memcpy(&jb->buff[ROUND_DOWN2(jb->free, jnl_fs_block_size) + jb->buff_off], eof_rec_buffer, adjust);
	}
	jb->filesize = header->virtual_size;
	jb->min_write_size = JNL_MIN_WRITE;
	jb->max_write_size = JNL_MAX_WRITE;
	jb->before_images = header->before_images;
	jb->epoch_tn = eof_record.prefix.tn;
	csd->jnl_checksum = header->checksum;
	LOG2_OF_INTEGER(header->alignsize, jb->log2_of_alignsize);
	assert(header->autoswitchlimit == csd->autoswitchlimit);
	assert(header->jnl_alq == csd->jnl_alq);
	assert(header->jnl_deq == csd->jnl_deq);
	assert(csd->autoswitchlimit >= csd->jnl_alq);
	assert(ALIGNED_ROUND_UP(csd->autoswitchlimit, csd->jnl_alq, csd->jnl_deq) == csd->autoswitchlimit);
	assert(csd->autoswitchlimit);
	JNL_WHOLE_TIME(prc_vec->jpv_time);
	jb->epoch_interval = header->epoch_interval;
	jb->next_epoch_time = (uint4)(MID_TIME(prc_vec->jpv_time) + jb->epoch_interval);
	jb->max_jrec_len = header->max_jrec_len;
	memcpy(&header->who_opened, prc_vec, SIZEOF(jnl_process_vector));
	header->crash = TRUE;	/* in case this processes is crashed, this will remain TRUE */
	VMS_ONLY(
		if (REPL_ENABLED(csd) && pool_init)
			header->update_disabled = jnlpool_ctl->upd_disabled;
	)
	JNL_DO_FILE_WRITE(csa, csd->jnl_file_name, jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
	{
		assert(WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC));
		return ERR_JNLWRERR;
	}
	if (!jb->prev_jrec_time || !header->prev_jnl_file_name_length)
	{	/* This is the first time a journal file for this database is being opened OR the previous link is NULL.
		 * In both these cases, we dont know or care about the timestamp of the last written journal record.
		 * Set it to the current time as we know it.
		 */
		jb->prev_jrec_time = jgbl.gbl_jrec_time;
	}
	jb->end_of_data = 0;
	jb->eov_tn = 0;
	jb->eov_timestamp = 0;
	jb->end_seqno = 0;
	return 0;
}
