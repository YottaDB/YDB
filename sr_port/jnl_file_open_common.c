/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include <errno.h>

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "interlock.h"
#include "lockconst.h"
#include "aswp.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "gtmio.h"
#include "sgtm_putmsg.h"
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

GBLREF	int			pool_init;
GBLREF  jnl_process_vector      *prc_vec;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	uint4			mu_reorg_encrypt_in_prog;

error_def(ERR_CRYPTJNLMISMATCH);
error_def(ERR_FILEIDMATCH);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLOPNERR);
error_def(ERR_JNLPREVRECOV);
error_def(ERR_JNLRDERR);
error_def(ERR_JNLRECTYPE);
error_def(ERR_JNLSWITCHRETRY);
error_def(ERR_JNLTRANSGTR);
error_def(ERR_JNLTRANSLSS);
error_def(ERR_JNLVSIZE);
error_def(ERR_JNLWRERR);
error_def(ERR_PREMATEOF);

#define RETURN_AND_SET_JPC(ERR, ERR2, BUF)	\
MBSTART {					\
	jpc->status = ERR2;			\
	jpc->err_str = BUF;			\
	return ERR;				\
} MBEND

/* note: returns 0 on success */
uint4 jnl_file_open_common(gd_region *reg, off_jnl_t os_file_size, char *buff)
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
	uint4			end_of_data, jnl_fs_block_size, read_write_size, read_size;
	int4			status;
	gtm_uint64_t		header_virtual_size;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	jpc = csa->jnl;
	jb = jpc->jnl_buff;
	jpc->status = jpc->status2 = SS_NORMAL;
	assert(NULL != buff);
	jnl_fs_block_size = get_fs_block_size(jpc->channel);
	/* check that the filesystem block size is a power of 2 as we do a lot of calculations below assuming this is the case */
	assert(!(jnl_fs_block_size & (jnl_fs_block_size - 1)));
	/* Ensure filesystem-block-size alignment except in case the filesystem-block-size is higher than os-page-size (not
	 * currently seen on the popular filesystems but seen in NFS filesystems). In that case only ensure os-page-size-alignment
	 * since processes that attach to database shared memory attach at os-page-size-aligned virtual addresses. So treat
	 * filesystem-block-size as the os-page-size in that case.
	 */
	if (OS_PAGE_SIZE < jnl_fs_block_size)
		jnl_fs_block_size = OS_PAGE_SIZE;
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
		assert(ERR_PREMATEOF == jpc->status);
		return ERR_JNLRDERR; /* Has one !AD parameter, the journal file, which jnl_send_oper() provides */
	}
	/* Check if the header format matches our format. Cannot access any fields inside header unless this matches */
	CHECK_JNL_FILE_IS_USABLE(header, jpc->status, FALSE, 0, NULL);	/* FALSE => NO gtm_putmsg even if errors */
	if (SS_NORMAL != jpc->status)
	{
		sgtm_putmsg(buff, VARLSTCNT(7) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), jpc->status);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, jpc->status, buff);
	}
	end_of_data = header->end_of_data;
	adjust = end_of_data & (jnl_fs_block_size - 1);
	/* Read the journal JRT_EOF at end_of_data offset.
	 * Make sure the buffer being read to is big enough and that as part of the read,
	 * we never touch touch the journal file header territory.
	 */
	read_size = ROUND_UP2((EOF_RECLEN + adjust), jnl_fs_block_size);
	assert(eof_rec_buffer + read_size <= ARRAYTOP(eof_rec));
	assert(end_of_data - adjust >= JNL_HDR_LEN);
	DO_FILE_READ(jpc->channel, end_of_data - adjust, eof_rec_buffer, read_size, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
		return ERR_JNLRDERR; /* Has one !AD parameter, the journal file, which jnl_send_oper() provides */
	if (header->prev_recov_end_of_data)
	{
		/* not possible for run time. In case it happens user must fix it */
		sgtm_putmsg(buff, VARLSTCNT(7) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), ERR_JNLPREVRECOV);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, ERR_JNLPREVRECOV, buff);
	}
	if (!is_gdid_file_identical(&FILE_ID(reg), (char *)header->data_file_name, header->data_file_name_length))
	{
		rts_error_csa(CSA_ARG(csa) VARLSTCNT(7) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), ERR_FILEIDMATCH);
		assert(FALSE);	/* we don't expect the rts_error in the line above to return */
		return ERR_JNLOPNERR;
	}
	memcpy(&eof_record, (unsigned char *)eof_rec_buffer + adjust, EOF_RECLEN);
	if (JRT_EOF != eof_record.prefix.jrec_type)
	{
		sgtm_putmsg(buff, VARLSTCNT(7) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg), ERR_JNLRECTYPE);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, ERR_JNLRECTYPE, buff);
	}
	if (eof_record.prefix.tn != csd->trans_hist.curr_tn)
	{
		if (eof_record.prefix.tn < csd->trans_hist.curr_tn)
			status = ERR_JNLTRANSLSS;
		else
			status = ERR_JNLTRANSGTR;
		sgtm_putmsg(buff, VARLSTCNT(10) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg),
				status, 2, &eof_record.prefix.tn, &csd->trans_hist.curr_tn);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, status, buff);
	}
	if (eof_record.suffix.suffix_code != JNL_REC_SUFFIX_CODE ||
		eof_record.suffix.backptr != eof_record.prefix.forwptr)
	{
		sgtm_putmsg(buff, VARLSTCNT(11) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg),
				ERR_JNLBADRECFMT, 3, JNL_LEN_STR(csd), adjust);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, ERR_JNLBADRECFMT, buff);
	}
	if (!mu_reorg_encrypt_in_prog && !SAME_ENCRYPTION_SETTINGS(header, csd))
	{	/* We expect encryption settings in the journal to be in sync with those in the file header. The only exception is
		 * MUPIP REORG -ENCRYPT, which switches the journal file upon changing encryption-specific fields in the file
		 * header, thus temporarily violating this expectation.
		 */
		sgtm_putmsg(buff, VARLSTCNT(12) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg),
				ERR_CRYPTJNLMISMATCH, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg));
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, ERR_CRYPTJNLMISMATCH, buff);
	}
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
		sgtm_putmsg(buff, VARLSTCNT(14) ERR_JNLOPNERR, 4, JNL_LEN_STR(csd), DB_LEN_STR(reg),
				ERR_JNLVSIZE, 7, JNL_LEN_STR(csd), header->virtual_size,
					header->jnl_alq, header->jnl_deq, os_file_size, jnl_fs_block_size);
		RETURN_AND_SET_JPC(ERR_JNLOPNERR, ERR_JNLVSIZE, buff);
	}
	/* For performance reasons (to be able to do aligned writes to the journal file), we need to ensure the journal buffer
	 * address is filesystem-block-size aligned in Unix. Although this is needed only in case of sync_io/direct-io, we ensure
	 * this alignment unconditionally in Unix. jb->buff_off is the number of bytes to go past before getting an aligned buffer.
	 */
	jb->buff_off = (uintszofptr_t)ROUND_UP2((uintszofptr_t)&jb->buff[0], jnl_fs_block_size) - (uintszofptr_t)&jb->buff[0];
	jb->size = ROUND_DOWN2(csd->jnl_buffer_size * DISK_BLOCK_SIZE - jb->buff_off, jnl_fs_block_size);
	/* Assert that journal buffer does NOT spill past the allocated journal buffer size in shared memory */
	assert((sm_uc_ptr_t)&jb->buff[jb->buff_off + jb->size] < ((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE(csd)
											+ JNL_SHARE_SIZE(csd)));
	assert((sm_uc_ptr_t)jb == ((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE(csd) + JNL_NAME_EXP_SIZE));
	jb->last_eof_written = header->last_eof_written;
	jb->freeaddr = jb->dskaddr = jb->fsync_dskaddr = end_of_data;
	SET_JBP_RSRV_FREEADDR(jb, end_of_data);	/* sets both jb->rsrv_freeaddr & jb->rsrv_free */
	jb->post_epoch_freeaddr = jb->end_of_data_at_open = jb->freeaddr;
	jb->fs_block_size = jnl_fs_block_size;
	/* The following is to make sure that the data in jnl_buffer is aligned with the data in the
	 * disk file on an jnl_fs_block_size boundary. Since we assert that jb->size is a multiple of jnl_fs_block_size,
	 * alignment with respect to jb->size implies alignment with jnl_fs_block_size.
	 */
	assert(0 == (jb->size % jnl_fs_block_size));
	jb->free = jb->dsk = end_of_data % jb->size;
	SET_LATCH_GLOBAL(&jb->fsync_in_prog_latch, LOCK_AVAILABLE);
	SET_LATCH_GLOBAL(&jb->io_in_prog_latch, LOCK_AVAILABLE);
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
	assert(4 == SIZEOF(header->alignsize));
	assert(((gtm_uint64_t)JNL_MAX_ALIGNSIZE * 512) < (gtm_uint64_t)MAXUINT4); /* ensure 4-byte "alignsize" will not overflow */
	jb->alignsize = header->alignsize;
	/* Use "gtm_uint64_t" typecast below to avoid 4G overflow issues with the ROUND_UP2 */
	jb->next_align_addr = (uint4)(ROUND_UP2(((gtm_uint64_t)jb->freeaddr + MIN_ALIGN_RECLEN), header->alignsize)
													- MIN_ALIGN_RECLEN);
	SET_LATCH_GLOBAL(&jb->phase2_commit_latch, LOCK_AVAILABLE);
	jb->phase2_commit_index1 = jb->phase2_commit_index2 = 0;
	/* The below 2 lines are relied upon by "mutex_salvage" */
	jb->phase2_commit_array[JNL_PHASE2_COMMIT_ARRAY_SIZE - 1].start_freeaddr = jb->end_of_data_at_open;
	jb->phase2_commit_array[JNL_PHASE2_COMMIT_ARRAY_SIZE - 1].tot_jrec_len = 0;
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
	if (header->is_not_latest_jnl)
	{	/* Magic message to indicate that we should try a switch without cutting links. */
		RETURN_AND_SET_JPC(ERR_JNLSWITCHRETRY, 0, buff);
	}
	header->crash = TRUE;	/* in case this processes is crashed, this will remain TRUE */
	JNL_DO_FILE_WRITE(csa, csd->jnl_file_name, jpc->channel, 0, header, read_write_size, jpc->status, jpc->status2);
	if (SS_NORMAL != jpc->status)
	{
		assert(WBTEST_ENABLED(WBTEST_RECOVER_ENOSPC));
		return ERR_JNLWRERR;
	}
	SET_JNLBUFF_PREV_JREC_TIME(jb, eof_record.prefix.time, DO_GBL_JREC_TIME_CHECK_FALSE);
		/* Sets jb->prev_jrec_time to time of to-be-overwritten EOF rec */
	jb->end_of_data = 0;
	jb->eov_tn = 0;
	jb->eov_timestamp = 0;
	jb->end_seqno = 0;
	return 0;
}
