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
error_def(ERR_JNLFILETOOBIG);
error_def(ERR_DBFILERR);
error_def(ERR_NEWJNLFILECREATE);
error_def(ERR_DSKSPACEFLOW);
error_def(ERR_JNLDBERR);


int jnl_file_extend(jnl_private_control *jpc, uint4 total_jnl_rec_size)
{
	file_control		*fc;
	jnl_create_info 	jnl_info;
	off_t			old_end;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	char			*buff, *fn,
				full_db_fn[MAX_FN_LEN], jnl_file_name[JNL_NAME_SIZE], prev_jnl_fn[JNL_NAME_SIZE];
	unsigned short		fn_len;
	int4			blk, wrt_size;
	int4			cond = 0;
	uint4			dbfilop(), jnl_status = 0, status, save_errno;
	int			new_blocks, result;
	GTM_BAVAIL_TYPE		avail_blocks;

	csa = &FILE_INFO(jpc->region)->s_addrs;
	csd = csa->hdr;
	assert(csa == cs_addrs && csd == cs_data);
	assert(csa->now_crit);
	assert(jpc->region == gv_cur_region);
	if (!JNL_ENABLED(csa->hdr) || (NOJNL == jpc->channel)
		|| !is_gdid_gdid_identical(&csa->jnl->fileid, &csd->jnl_file.u))
			GTMASSERT;	/* crit and messing with the journal file - how could it have vanished? */
	new_blocks = ROUND_UP(DIVIDE_ROUND_UP(total_jnl_rec_size, DISK_BLOCK_SIZE), csa->hdr->jnl_deq);
	jpc->status = 0;
	if (0 != (save_errno = disk_block_available(jpc->channel, &avail_blocks, TRUE)))
		send_msg(VARLSTCNT(5) ERR_JNLDBERR, 2, JNL_LEN_STR(csa->hdr), save_errno);
	else
	{
		if ((new_blocks * EXTEND_WARNING_FACTOR) > avail_blocks)
			send_msg(VARLSTCNT(5) ERR_DSKSPACEFLOW, 3, JNL_LEN_STR(csa->hdr),
					(uint4)(avail_blocks - ((new_blocks <= avail_blocks) ? new_blocks : 0)));
	}

	if (0 < new_blocks)
	{
		old_end = jpc->jnl_buff->filesize + total_jnl_rec_size;
		/* assert(0 == (old_end & ~JNL_WRT_START_MASK));	file must always be an even number of blocks */
		if ((jpc->jnl_buff->autoswitchlimit - (old_end / DISK_BLOCK_SIZE) + 1) <= (EXTEND_WARNING_FACTOR * new_blocks))
		{	/* close to max */
			if ((jpc->jnl_buff->autoswitchlimit - (old_end / DISK_BLOCK_SIZE) + 1) <= new_blocks)
			{	/* reaching max */
				if ((jpc->jnl_buff->autoswitchlimit - (old_end / DISK_BLOCK_SIZE) + 1) <= 0)
				{	/* the file somehow exceeded the design, probably compromising integrity - trigger close */
					jpc->status = ERR_JNLFILETOOBIG;
					new_blocks = -1;
				} else
				{	/* time for a new file */
					memset(&jnl_info, 0, sizeof(jnl_info));
					jnl_info.prev_jnl = &prev_jnl_fn[0];
					set_jnl_info(gv_cur_region, &jnl_info);
					fc = gv_cur_region->dyn.addr->file_cntl;
					assert((JNL_ENABLED(csa->hdr) && (NOJNL != jpc->channel)
						&& is_gdid_gdid_identical(&csa->jnl->fileid, &csd->jnl_file.u)));
					jnl_status = jnl_ensure_open();
					if (0 == jnl_status)
					{
						wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_WRITE_EPOCH);
						jnl_file_close(gv_cur_region, TRUE, TRUE);
					} else
					{
						rts_error(VARLSTCNT(6) jnl_status, 4, JNL_LEN_STR(csd),
							DB_LEN_STR(gv_cur_region));
					}
					if (0 == cre_jnl_file(&jnl_info))
					{
						memcpy(csd->jnl_file_name, jnl_info.jnl, jnl_info.jnl_len);
						csd->jnl_file_name[jnl_info.jnl_len] = '\0';
						csd->jnl_file_len = jnl_info.jnl_len;
						csd->jnl_buffer_size = jnl_info.buffer;
						csd->jnl_alq = jnl_info.alloc;
						csd->jnl_deq = jnl_info.extend;
						csd->jnl_before_image = jnl_info.before_images;
						csd->trans_hist.header_open_tn = jnl_info.tn;
						send_msg(VARLSTCNT(3) ERR_NEWJNLFILECREATE, 2,
							JNL_LEN_STR(csa->hdr));
						fc->op = FC_WRITE;
						fc->op_buff = (sm_uc_ptr_t)csd;
						status = dbfilop(fc);
						if (SS_NORMAL != status)
						{
							send_msg(VARLSTCNT(5) ERR_DBFILERR, 2,
								gv_cur_region->dyn.addr->fname_len,
								gv_cur_region->dyn.addr->fname, status);
						}
						assert(JNL_ENABLED(csa->hdr));
						/* call jnl_ensure_open instead of jnl_file_open to make sure
						 * 	cs_addrs->jnl->pini_addr is set to 0
						 */
						jnl_status = jnl_ensure_open();	/* sets jpc->status */
						if (jnl_status != 0)
							rts_error(VARLSTCNT(6) jnl_status, 4, csa->hdr->jnl_file_len,
									csa->hdr->jnl_file_name, DB_LEN_STR(gv_cur_region));
					} else
					{
						jpc->status = ERR_JNLEXTEND;
						send_msg(VARLSTCNT(5) ERR_JNLEXTEND, 2,
							JNL_LEN_STR(csa->hdr), jnl_info.status);
					}
				}
			} else
				send_msg(VARLSTCNT(5) ERR_JNLSPACELOW, 3, JNL_LEN_STR(csa->hdr),
					jpc->jnl_buff->autoswitchlimit - (old_end / DISK_BLOCK_SIZE) + 1 - new_blocks);
		}
	} else
	{
		new_blocks = -1;
		jpc->status = ERR_JNLREADEOF;
	}
	if ((0 == jpc->status) && (0 < new_blocks))
		jpc->jnl_buff->filesize += new_blocks * DISK_BLOCK_SIZE;
	else
	{
		jnl_file_lost(jpc, ERR_JNLEXTEND);
		new_blocks = -1;
	}
	return new_blocks;
}
