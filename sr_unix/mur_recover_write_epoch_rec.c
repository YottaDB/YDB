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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "jnl.h"
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "copy.h"
#include "gtmio.h"
#include "gdskill.h"
#include "collseq.h"
#include "gdscc.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "parse_file.h"
#include "lockconst.h"
#include "aswp.h"
#include "eintr_wrappers.h"
#include "io_params.h"
#include "rename_file_if_exists.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "util.h"
#include "gtmmsg.h"
#include "wcs_flu.h"
#include "tp_change_reg.h"

GBLREF	boolean_t	jnlfile_truncation;
GBLREF	gd_region	*gv_cur_region;
GBLREF  sgmnt_addrs     *cs_addrs;
GBLREF  sgmnt_data_ptr_t cs_data;

void mur_recover_write_epoch_rec(ctl_list **jnl_files)
{
	ctl_list		*curr;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t     	jb;
	struct	stat		stat_buf;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	int			fd, ftruncate_res;
	int4			info_status;
	uint4			status, n;
	char			*cmd, temp_file_name[MAX_FN_LEN + 1], cmd_string[MAX_LINE], rename_fn[MAX_FN_LEN + 1];
	int			fstat_res, temp_file_name_len, rename_len, rv;
	boolean_t		proceed;

	error_def(ERR_JNLREADEOF);
	error_def(ERR_RENAMEFAIL);

	for (curr = *jnl_files;  curr != NULL;  curr = curr->next)
	{	/* If 0 == curr->consist_stop_addr, rollback-like handling needs to be done
		 * 	i.e. rename journal to _rolled_bak.
		 * Not done now due to V4.2 time pressure.
		 * Also db-file-header should be updated to reflect the older generation journal file name.
		 * The whole rollback/recover mechanism needs a code-review after V4.2 gets released.
		 */
		if (curr->found_eof || 0 == curr->consist_stop_addr)
			continue;
		fd = curr->rab->pvt->fd;
		if (curr->rab->pvt->last_record != curr->consist_stop_addr)
		{
			if (SS_NORMAL != (status = mur_next(curr->rab, curr->consist_stop_addr)))
				mur_jnl_read_error(curr, status, TRUE);
			for ( proceed = TRUE; proceed; )
			{
				if (SS_NORMAL != (status = mur_next(curr->rab, 0))  &&  ERR_JNLREADEOF != status)
				{
					mur_jnl_read_error(curr, status, TRUE);
					break;
				}
				if (ERR_JNLREADEOF == status)
				{
					curr->consist_stop_addr = curr->rab->dskaddr;
					curr->rab->dskaddr += curr->rab->reclen;
					break;
				}
				switch (REF_CHAR(&((jnl_record *)curr->rab->recbuff)->jrec_type))
				{
					case JRT_EPOCH:
					case JRT_PINI:
					case JRT_PFIN:
					case JRT_ALIGN:
						curr->consist_stop_addr = curr->rab->dskaddr;
						break;
					default:
						proceed = FALSE;
						break;
				}
			}
			n = ROUND_UP(curr->rab->dskaddr + curr->rab->reclen, JNL_REC_START_BNDRY);

			/* Take a backup of this journal file since this is going to be truncated */
			memcpy(temp_file_name, curr->jnl_fn, curr->jnl_fn_len);
			memcpy((char *)temp_file_name + curr->jnl_fn_len, RECOVERSUFFIX, sizeof(RECOVERSUFFIX) - 1);
			temp_file_name_len = curr->jnl_fn_len + sizeof(RECOVERSUFFIX) - 1;
			temp_file_name[temp_file_name_len] = '\0'; /* Rename_file_if_exists expects null termination */

			cmd = cmd_string;
			memcpy(cmd, BKUP_CMD, sizeof(BKUP_CMD) - 1);
			cmd += sizeof(BKUP_CMD) - 1;

			memcpy(cmd, curr->jnl_fn, curr->jnl_fn_len);
			cmd += curr->jnl_fn_len;

			*cmd++ = ' ';

			memcpy(cmd, temp_file_name, temp_file_name_len);
			cmd += temp_file_name_len;
			*cmd = '\0';
			if (RENAME_FAILED == rename_file_if_exists(temp_file_name, temp_file_name_len, &info_status,
                                rename_fn, &rename_len))
                                gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 4, temp_file_name_len, temp_file_name,
                                        rename_len, rename_fn, info_status);
			if (0 != (rv = SYSTEM(cmd_string)))
			{
				jnlfile_truncation = FALSE;
				if (-1 == rv)
					PERROR("copy : ");
				util_out_print("MUR-E-ERRCOPY : Error backing up jnl file !AD to !AD", TRUE,
					curr->jnl_fn_len, curr->jnl_fn, temp_file_name_len, temp_file_name);
				/* Even though the copy failed we continue with the file truncation */
			}
			jnlfile_truncation = TRUE;
			FTRUNCATE(fd, (off_t)n, ftruncate_res);
			if (0 != ftruncate_res)
			{
				PERROR("");
				util_out_print("MUR-E-ERRTRUNC : Failed to truncate file !AD to length !UL",
							TRUE, curr->jnl_fn_len, curr->jnl_fn, n + EOF_RECLEN);
			}
		} else
		{
			if (SS_NORMAL != (status = mur_next(curr->rab, curr->consist_stop_addr)))
				mur_jnl_read_error(curr, status, TRUE);
			n = ROUND_UP(curr->rab->dskaddr + curr->rab->reclen, JNL_REC_START_BNDRY);
		}
		gv_cur_region = curr->gd;
		tp_change_reg();
		csa = cs_addrs;
		csd = cs_data;

		/* Note: the following code works on the assumption that no one is operating on the journal buffer and it has
		 * not been initialised. The initialization part should parallel the code in jnl_file_open at all times.  */
		assert(!JNL_ALLOWED(csd));
		wcs_flu(WCSFLU_FLUSH_HDR); 		/* since we know journalling is disabled, no need to write epoch */
		/* Assign csa->jnl before enabling journalling/replication. This is because there are lots of places
		 * which assume that if journalling is enabled, then csa->jnl should be non-zero (including wcs_wtstart
		 * which is a timer-routine and hence can otherwise be triggered in the small window between csa->jnl
		 * and csd->jnl_state assignments.
		 */
		if (NULL == csa->jnl)
		{
			csa->jnl = (jnl_private_control *)malloc(sizeof(*csa->jnl));
			memset(csa->jnl, 0, sizeof(*csa->jnl));
			csa->jnl->region = curr->gd;
		}
		csd->jnl_state = jnl_open;		/* temporarily enable journalling so jnl_flush will work right */
		jpc = csa->jnl;
      		csa->jnl->jnl_buff = (jnl_buffer_ptr_t)((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE + JNL_NAME_EXP_SIZE);
		++csa->ti->curr_tn;
		cs_data->trans_hist.early_tn = csa->ti->curr_tn;
		jb = jpc->jnl_buff;
		jpc->channel = fd;
		FSTAT_FILE(jpc->channel, &stat_buf, fstat_res);
		jpc->status = 0;
		jb->size = csd->jnl_buffer_size * DISK_BLOCK_SIZE;
		jb->freeaddr = jb->dskaddr = jb->fsync_dskaddr = n;
		jb->lastaddr = curr->consist_stop_addr;
		jb->free = jb->dsk = jb->freeaddr % jb->size;
		SET_LATCH_GLOBAL(&jb->fsync_in_prog_latch, LOCK_AVAILABLE);
		SET_LATCH_GLOBAL(&jb->io_in_prog_latch, LOCK_AVAILABLE);
		jb->filesize = stat_buf.st_size;
		jb->min_write_size = JNL_MIN_WRITE;
		jb->max_write_size = JNL_MAX_WRITE;
		jb->alignsize = curr->rab->pvt->jfh->alignsize;
		jb->epoch_interval = curr->rab->pvt->jfh->epoch_interval;

		/* assign fileid to some non-zero value so that jnl_file_close won't assert fail */
		jpc->fileid.inode = jpc->fileid.device = jpc->fileid.st_gen =
		csd->jnl_file.u.inode = csd->jnl_file.u.device = csd->jnl_file.u.st_gen = 10;

		grab_crit(gv_cur_region);
		jnl_put_jrt_pini(csa);
		jnl_write_epoch_rec(csa);
		jnl_put_jrt_pfin(csa);
		jnl_file_close(jpc->region, TRUE, FALSE);
		jnlfile_truncation = FALSE;
		rel_crit(gv_cur_region);
		csd->jnl_state = jnl_notallowed;	/* reset jnl_state to where it was */
	}
}
