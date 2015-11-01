/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include "gdsroot.h"
#include "gtm_statvfs.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "jnl.h"
#include "gtm_stat.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "send_msg.h"
#include "is_file_identical.h"
#include "dbfilop.h"
#include "disk_block_available.h"
#include "wcs_flu.h"

#define BLKS_PER_WRITE	64

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

error_def(ERR_JNLEXTEND);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLSPACELOW);
error_def(ERR_NEWJNLFILECREATE);
error_def(ERR_DSKSPACEFLOW);
error_def(ERR_JNLFILEXTERR);
error_def(ERR_JNLCREATERR);
error_def(ERR_DBFILERR);

int jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size)
{
	file_control		*fc;
	boolean_t		need_extend;
	jnl_buffer_ptr_t     	jb;
	jnl_create_info 	jnl_info;
	uint4			new_alq;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char			*buff, *fn, full_db_fn[MAX_FN_LEN], jnl_file_name[JNL_NAME_SIZE], prev_jnl_fn[JNL_NAME_SIZE];
	char			zero_disk_block[DISK_BLOCK_SIZE];
	unsigned short		fn_len;
	int4			blk, wrt_size;
	int4			cond = 0;
	uint4			jnl_status = 0, status, save_errno;
	int			new_blocks, result;
	GTM_BAVAIL_TYPE		avail_blocks;
	uint4			aligned_tot_jrec_size, count;

	csa = &FILE_INFO(jpc->region)->s_addrs;
	csd = csa->hdr;
	assert(csa == cs_addrs && csd == cs_data);
	assert(csa->now_crit);
	assert(jpc->region == gv_cur_region);
	if (!JNL_ENABLED(csa) || (NOJNL == jpc->channel)
		|| !is_gdid_gdid_identical(&csa->jnl->fileid, &csa->nl->jnl_file.u))
			GTMASSERT;	/* crit and messing with the journal file - how could it have vanished? */
	if (!csd->jnl_deq)
	{
		assert(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE) <= csd->jnl_alq);
		new_blocks = csd->jnl_alq;
	} else
		new_blocks = ROUND_UP(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE), csd->jnl_deq);
	jpc->status = SS_NORMAL;
	jb = jpc->jnl_buff;
	assert(0 <= new_blocks);
	DEBUG_ONLY(count = 0);
	for (need_extend = new_blocks; need_extend; )
	{
		DEBUG_ONLY(count++);
		/* usually we will do the loop just once where we do the file extension.
		 * rarely we might need to do an autoswitch instead after which again rarely
		 * 	we might need to do an extension on the new journal to fit in the transaction's	journal requirements.
		 * therefore we should do this loop a maximum of twice. hence the assert below.
		 */
		assert(count <= 2);
		need_extend = FALSE;
		if (0 != (save_errno = disk_block_available(jpc->channel, &avail_blocks, TRUE)))
			send_msg(VARLSTCNT(5) ERR_JNLFILEXTERR, 2, JNL_LEN_STR(csd), save_errno);
		else
		{
			if ((new_blocks * EXTEND_WARNING_FACTOR) > avail_blocks)
				send_msg(VARLSTCNT(5) ERR_DSKSPACEFLOW, 3, JNL_LEN_STR(csd),
						(uint4)(avail_blocks - ((new_blocks <= avail_blocks) ? new_blocks : 0)));
		}
		new_alq = jb->filesize + new_blocks;
		/* ensure current journal file size is well within autoswitchlimit --> design constraint */
		assert(jb->autoswitchlimit >= jb->filesize);
		if (jb->autoswitchlimit < (jb->filesize + (EXTEND_WARNING_FACTOR * new_blocks)))	/* close to max */
			send_msg(VARLSTCNT(5) ERR_JNLSPACELOW, 3, JNL_LEN_STR(csd), jb->autoswitchlimit - jb->filesize);
		if (jb->autoswitchlimit < new_alq)
		{	/* reached max */
			/* ensure new jnl file can hold the entire current transaction's journal record requirements */
			assert(jb->autoswitchlimit >= MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size));
			/* time for a new file */
			memset(&jnl_info, 0, sizeof(jnl_info));
			jnl_info.prev_jnl = &prev_jnl_fn[0];
			set_jnl_info(gv_cur_region, &jnl_info);
			assert((JNL_ENABLED(csa) && (NOJNL != jpc->channel)
				&& is_gdid_gdid_identical(&csa->jnl->fileid, &csa->nl->jnl_file.u)));
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
				jnl_file_close(gv_cur_region, TRUE, TRUE);
			} else
				rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
			if (EXIT_NRM == cre_jnl_file(&jnl_info))
			{
				assert(0 == memcmp(csd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len));
				assert(csd->jnl_file_name[jnl_info.jnl_len] == '\0');
				assert(csd->jnl_file_len == jnl_info.jnl_len);
				assert(csd->jnl_buffer_size == jnl_info.buffer);
				assert(csd->jnl_alq == jnl_info.alloc);
				assert(csd->jnl_deq == jnl_info.extend);
				assert(csd->jnl_before_image == jnl_info.before_images);
				csd->trans_hist.header_open_tn = jnl_info.tn;	/* needed for successful jnl_file_open() */
				send_msg(VARLSTCNT(3) ERR_NEWJNLFILECREATE, 2, JNL_LEN_STR(csd));
				fc = gv_cur_region->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (sm_uc_ptr_t)csd;
				status = dbfilop(fc);
				if (SS_NORMAL != status)
					send_msg(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(gv_cur_region), status);
				assert(JNL_ENABLED(csa));
				/* call jnl_ensure_open instead of jnl_file_open to make sure jpc->pini_addr is set to 0 */
				jnl_status = jnl_ensure_open();	/* sets jpc->status */
				if (0 != jnl_status)
					rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd), DB_LEN_STR(gv_cur_region));
				aligned_tot_jrec_size = ALIGNED_ROUND_UP(MAX_REQD_JNL_FILE_SIZE(total_jnl_rec_size),
										csd->jnl_alq, csd->jnl_deq);
				if (aligned_tot_jrec_size > csd->jnl_alq)
				{	/* need to extend more than initial allocation in the new journal file
					 * to accommodate the current transaction.
					 */
					new_blocks = aligned_tot_jrec_size - csd->jnl_alq;
					assert(new_blocks);
					assert(0 == new_blocks % csd->jnl_deq);
					need_extend = TRUE;
				}
			} else
			{
				send_msg(VARLSTCNT(5) ERR_JNLCREATERR, 2, JNL_LEN_STR(csd), jnl_info.status);
				new_blocks = -1;
			}
		} else
		{
			jb->filesize += new_blocks;
			assert(DISK_BLOCK_SIZE == sizeof(zero_disk_block));
			memset(zero_disk_block, 0, DISK_BLOCK_SIZE);
			/* do type cast to off_t below to take care of 32-bit overflows due to multiplication by DISK_BLOCK_SIZE */
			LSEEKWRITE(jpc->channel, (((off_t)jb->filesize) - 1) * DISK_BLOCK_SIZE,
							zero_disk_block, DISK_BLOCK_SIZE, status);
			if (SS_NORMAL != status)
			{
				new_blocks = -1;
				jpc->status = status;
			}
			assert(!need_extend);	/* ensure we won't go through the for loop again */
		}
		if (0 >= new_blocks)
			break;
	}
	if (0 >= new_blocks)
	{
		jpc->status = ERR_JNLREADEOF;
		jnl_file_lost(jpc, ERR_JNLEXTEND);
	}
	return new_blocks;
}
