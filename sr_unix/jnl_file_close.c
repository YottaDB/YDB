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
#include "gtm_time.h"

#include <unistd.h>

#include "aswp.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gt_timer.h"
#include "filestruct.h"
#include "jnl.h"
#include "lockconst.h"
#include "interlock.h"
#include "gtmio.h"
#include "sleep_cnt.h"
#include "gdsblk.h"
#include "performcaslatchcheck.h"
#include "wcs_sleep.h"
#include "jnl_write.h"

#define	HDR_LEN		ROUND_UP(sizeof(jnl_file_header), 8)
GBLREF	boolean_t	mupip_jnl_recover;
GBLREF	boolean_t	jnlfile_truncation;
GBLREF	uint4		process_id;



#define	GET_IO_IN_PROG_LOCK(reg, csa, jb)									\
{														\
	int	lcnt, temp_count;										\
														\
	error_def(ERR_JNLQIOLOCKED);										\
	error_def(ERR_JNLCLOSE);										\
														\
	for (lcnt = 1; 												\
	     (lcnt < JNL_MAX_FLUSH_TRIES) && (FALSE == GET_SWAPLOCK(&jb->io_in_prog_latch));				\
	     lcnt++)												\
        {													\
		wcs_sleep(lcnt);										\
                performCASLatchCheck(&jb->io_in_prog_latch);							\
        }													\
	if (lcnt == JNL_MAX_FLUSH_TRIES)									\
	{													\
		assert(FALSE);											\
		jpc->status = 0;										\
		jnl_send_oper(csa->jnl, ERR_JNLQIOLOCKED);							\
		rts_error(VARLSTCNT(6) ERR_JNLCLOSE, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(reg));		\
	}													\
}

#define	RELEASE_IO_IN_PROG_LOCK(jb) RELEASE_SWAPLOCK(&jb->io_in_prog_latch)

void	jnl_file_close(gd_region *reg, bool clean, bool eov)
{
	sgmnt_addrs		*csa;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	jnl_file_header		*header;
	struct_jrec_eof		eof_record;
	uint4			end_of_data, status = 0;
	int4			zero_len;
	char			hdr_buffer[HDR_LEN];
	char			zeroes[DISK_BLOCK_SIZE];

	error_def(ERR_JNLEOFPREZERO);
	error_def(ERR_JNLCLOSE);

	csa = &FILE_INFO(reg)->s_addrs;
	assert(csa->now_crit);
	assert(0 != csa->hdr->jnl_file.u.inode);
	assert(0 != csa->hdr->jnl_file.u.device);

	jpc = csa->jnl;
	if ((NULL == jpc) || (NOJNL == jpc->channel))
		return;
	if (csa->dbsync_timer)
	{
		cancel_timer((TID)csa);
		csa->dbsync_timer = FALSE;
	}
	jnl_flush(reg);
	jb = jpc->jnl_buff;
	assert(jb->dskaddr == jb->freeaddr);
	if (clean)
	{	/* Get the qio_in_prog lock before updating dskaddr/dsk */
		GET_IO_IN_PROG_LOCK(reg, csa, jb);
		end_of_data = jb->freeaddr = ROUND_UP(jb->freeaddr, DISK_BLOCK_SIZE);
		zero_len = end_of_data - jb->dskaddr;
		assert(zero_len >= 0 && DISK_BLOCK_SIZE > zero_len);
		jpc->status = 0;
		if (zero_len > 0)
		{
			memset(zeroes, 0, zero_len);
			LSEEKWRITE(jpc->channel, jb->dskaddr, zeroes, zero_len, jpc->status);
			if (0 != jpc->status)
			{
				jnl_send_oper(jpc, ERR_JNLEOFPREZERO);
				RELEASE_IO_IN_PROG_LOCK(jb);
				rts_error(VARLSTCNT(6) ERR_JNLCLOSE, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(reg));
			}
		}
		jb->dskaddr = jb->freeaddr;
		jb->free = jb->free + zero_len;
		if (jb->free >= jb->size)
		{
			assert(jb->free == jb->size);
			jb->free = 0;
		}
		jb->dsk = jb->free;
		assert(0 == jb->free % DISK_BLOCK_SIZE);
		assert(jb->free == jb->freeaddr % jb->size);
		RELEASE_IO_IN_PROG_LOCK(jb);

		/* Write an EOF record */
		jnl_prc_vector(&eof_record.process_vector);
		eof_record.tn = csa->ti->curr_tn;
		QWASSIGN(eof_record.jnl_seqno, csa->hdr->reg_seqno);
		jnl_write(jpc, JRT_EOF, (jrec_union *)&eof_record, NULL, NULL);
		jnl_flush(reg);
		jnl_fsync(reg, jb->dskaddr);
		assert(jb->dskaddr == jb->freeaddr);
		assert(jb->dskaddr == jb->fsync_dskaddr);

		GET_IO_IN_PROG_LOCK(reg, csa, jb);
		if (0 == jpc->status)
			LSEEKREAD(jpc->channel, 0, (sm_uc_ptr_t)hdr_buffer, sizeof(hdr_buffer), jpc->status);
		if (0 == jpc->status)
		{
			header = (jnl_file_header *)hdr_buffer;
			assert(jnlfile_truncation || header->end_of_data <= end_of_data);
			header->end_of_data = end_of_data;
			JNL_SHORT_TIME(header->eov_timestamp);
			assert(header->eov_timestamp >= header->bov_timestamp);
			header->eov_tn = csa->ti->curr_tn;
			assert(header->eov_tn >= header->bov_tn);
			header->crash = FALSE;
			LSEEKWRITE(jpc->channel, 0, (sm_uc_ptr_t)hdr_buffer, sizeof(hdr_buffer), status);
		}
		RELEASE_IO_IN_PROG_LOCK(jb);
		/*
		 * jnl_file_id should be nullified only after the jnl file header has been written to disk.
		 * Nullifying the jnl_file_id signals that the jnl file has been switched. The replication source server
		 * assumes that the jnl file has been completely written to disk (including the header) before the switch is
		 * signalled.
		 */
		csa->hdr->jnl_file.u.inode = 0;
		csa->hdr->jnl_file.u.device = 0;
	}
	/* This will be closed as part of recover closing journal files in mur_close_files as this value is assigned in
	 * mur_recover_write_epoch_rec/mur_rollback_truncate as part of recover/rollback trying to write EOF/truncate */
	if (!mupip_jnl_recover)
		close(jpc->channel);
	jpc->channel = NOJNL;
	jpc->lastwrite = 0;
	jpc->regnum = 0;
	jpc->pini_addr = 0;
	if (clean && (0 != status))
	{
		jpc->status = status;        /* jnl_send_oper zeroes */
		jnl_send_oper(jpc, ERR_JNLCLOSE);
		rts_error(VARLSTCNT(7) ERR_JNLCLOSE, 4, JNL_LEN_STR(csa->hdr), DB_LEN_STR(reg), status);
	}
}
