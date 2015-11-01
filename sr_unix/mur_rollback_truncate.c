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
#include "tp_change_reg.h"
#include "wcs_flu.h"

GBLDEF	boolean_t	set_resync_to_region = FALSE;

GBLREF	boolean_t	jnlfile_truncation;
GBLREF	char		*log_rollback;
GBLREF	gd_region	*gv_cur_region;
GBLREF	seq_num		consist_jnl_seqno;
GBLREF  sgmnt_addrs     *cs_addrs;
GBLREF  sgmnt_data_ptr_t cs_data;

void	mur_rollback_truncate(ctl_list **jnl_files)
{
	boolean_t		proceed;
	char			*cmd, cmd_string[MAX_LINE], *errptr, temp_file_name[MAX_FN_LEN], rename_fn[MAX_FN_LEN];
	ctl_list		*ctl;
	int			db_fd, fd, fn_len, ftruncate_res, n, rv, save_errno, temp_file_name_len, rename_len;
	int4			info_status;
	jnl_buffer_ptr_t     	jb;
	jnl_file_header		*header;
	jnl_private_control	*jpc;
	struct	stat		stat_buf;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	uint4			curr_offset, status;

	error_def(ERR_JNLREADEOF);
	error_def(ERR_RENAMEFAIL);

	for (ctl = *jnl_files;  ctl != NULL;  ctl = ctl->prev)
	{
		fd = ctl->rab->pvt->fd;
		if (0 != ctl->consist_stop_addr)
		{
			assert(FALSE == ctl->concat_next  ||  NULL != ctl->next);
			if (TRUE == ctl->concat_next  &&  0 != ctl->next->consist_stop_addr)
				continue;
			if (SS_NORMAL != (status = mur_next(ctl->rab, ctl->consist_stop_addr)))
				mur_jnl_read_error(ctl, status, TRUE);
			for ( proceed = TRUE; TRUE == proceed; )
			{
				if (SS_NORMAL != (status = mur_next(ctl->rab, 0))  &&  ERR_JNLREADEOF != status)
				{
					mur_jnl_read_error(ctl, status, TRUE);
					break;
				}
				if (ERR_JNLREADEOF == status)
				{
					ctl->consist_stop_addr = ctl->rab->dskaddr;
					ctl->rab->dskaddr += ctl->rab->reclen;
					break;
				}
				switch(REF_CHAR(&((jnl_record *)ctl->rab->recbuff)->jrec_type))
				{
					case JRT_EPOCH:
					case JRT_PINI:
					case JRT_PFIN:
					case JRT_ALIGN:
						ctl->consist_stop_addr = ctl->rab->dskaddr;
						break;
					default:
						proceed = FALSE;
						break;
				}
			}

			gv_cur_region = ctl->gd;
			tp_change_reg();
			csa = cs_addrs;
			csd = cs_data;

			header = (jnl_file_header *)mur_get_file_header(ctl->rab);
			n = ROUND_UP(ctl->rab->dskaddr, DISK_BLOCK_SIZE);
			if (header->end_of_data != n || header->crash)
			{	/* Take a backup of this journal file since this is going to be truncated */

				memcpy(temp_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
				memcpy((char *)temp_file_name + ctl->jnl_fn_len, RLBKSUFFIX, sizeof(RLBKSUFFIX) - 1);
				temp_file_name_len = ctl->jnl_fn_len + sizeof(RLBKSUFFIX) - 1;

				cmd = cmd_string;
				memcpy(cmd, BKUP_CMD, sizeof(BKUP_CMD) - 1);
				cmd += sizeof(BKUP_CMD) - 1;

				memcpy(cmd, ctl->jnl_fn, ctl->jnl_fn_len);
				cmd += ctl->jnl_fn_len;

				*cmd++ = ' ';

				memcpy(cmd, temp_file_name, temp_file_name_len);
				cmd += temp_file_name_len;
				*cmd = '\0';

				if (0 != (rv = SYSTEM(cmd_string)))
				{
					if (-1 == rv)
					{
						save_errno = errno;
						errptr = (char *)STRERROR(save_errno);
						util_out_print(errptr, TRUE, save_errno);
					}
					util_out_print("MUR-E-ERRCOPY : Error backing up jnl file !AD to !AD", TRUE,
						ctl->jnl_fn_len, ctl->jnl_fn, temp_file_name_len, temp_file_name);
					/* Even though the copy failed we continue with the file truncation */
				}
				FTRUNCATE(fd, (off_t)(n + EOF_RECLEN), ftruncate_res);
				if (0 != ftruncate_res)
				{
					save_errno = errno;
					errptr = (char *)STRERROR(save_errno);
					util_out_print(errptr, TRUE, save_errno);
					util_out_print("MUR-E-ERRTRUNC : Failed to truncate file !AD to length !UL",
								TRUE, ctl->jnl_fn_len, ctl->jnl_fn, n + EOF_RECLEN);
				}
			}

			/* update the db file-header (if needed) to reflect the current jnl_file as the latest */
			assert(TRUE == ctl->concat_next  ||
				0 == memcmp(csa->hdr->jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len));
			if (TRUE == ctl->concat_next)
			{
				if (log_rollback)
					util_out_print("MUR-I-JNLFILECHNG : Database File !AD now uses jnl-file ---> !AD",
						TRUE, header->data_file_name_length, header->data_file_name,
							ctl->jnl_fn_len, ctl->jnl_fn);
				csa->hdr->jnl_file_len = ctl->jnl_fn_len;
				memcpy(csa->hdr->jnl_file_name, ctl->jnl_fn, ctl->jnl_fn_len);
				csa->hdr->jnl_file_name[ctl->jnl_fn_len] = '\0';
				/* the changes will be reflected to disk in mur_close_files in gds_rundown() */
			}
			/* Note: following code works on the assumption that no one is operating on the journal buffer and it has
			 * not been initialised. The initialization part should parallel the code in jnl_file_open at all times.
			 */
			wcs_flu(WCSFLU_FLUSH_HDR);	/* will write epoch down below. so just flush the cache */
			if (header->crash && header->update_disabled)
                                set_resync_to_region = TRUE;  /* Set resync_to_region seqno for a crash and update_disable case */
			/* Assign csa->jnl before enabling journalling/replication. This is because there are lots of places
			 * which assume that if journalling is enabled, then csa->jnl should be non-zero (including wcs_wtstart
			 * which is a timer-routine and hence can otherwise be triggered in the small window between csa->jnl
			 * and csd->jnl_state assignments.
			 */
			if (NULL == csa->jnl)
			{
				csa->jnl = (jnl_private_control *)malloc(sizeof(*csa->jnl));
				memset(csa->jnl, 0, sizeof(*csa->jnl));
				csa->jnl->region = ctl->gd;
			}
			csd->jnl_state = jnl_open;	/* temporarily enable journalling so jnl_flush will work right */
			csd->repl_state = repl_open;	/* enable replication so that reg_seqno is properly assigned
									in jnl_write_epoch_rec */
			jpc = csa->jnl;
      			jpc->jnl_buff = (jnl_buffer_ptr_t)((sm_uc_ptr_t)csa->nl + NODE_LOCAL_SPACE + JNL_NAME_EXP_SIZE);
			jb = jpc->jnl_buff;
			jpc->channel = fd;
			jpc->status = 0;
			jb->size = csd->jnl_buffer_size * DISK_BLOCK_SIZE;
			jb->freeaddr = jb->dskaddr = jb->fsync_dskaddr = ctl->rab->dskaddr;
			jb->lastaddr = ctl->consist_stop_addr;
			SET_LATCH_GLOBAL(&jb->fsync_in_prog_latch, LOCK_AVAILABLE);
			SET_LATCH_GLOBAL(&jb->io_in_prog_latch, LOCK_AVAILABLE);
			jb->free = jb->dsk = jb->freeaddr % jb->size;
			jb->filesize = jb->dskaddr;
			jb->min_write_size = JNL_MIN_WRITE;
			jb->max_write_size = JNL_MAX_WRITE;
			jb->alignsize = ctl->rab->pvt->jfh->alignsize;
			jb->epoch_interval = ctl->rab->pvt->jfh->epoch_interval;

			/* assign fileid to some non-zero value so that jnl_file_close won't assert fail */
			jpc->fileid.inode = jpc->fileid.device = jpc->fileid.st_gen =
				csd->jnl_file.u.inode = csd->jnl_file.u.device = csd->jnl_file.u.st_gen = 10;
			grab_crit(gv_cur_region);
			jnl_put_jrt_pini(csa);
			QWASSIGN(csd->reg_seqno, consist_jnl_seqno);	/* to write proper jnl_seqno in epoch record */
			jnl_write_epoch_rec(csa);
			jnl_put_jrt_pfin(csa);
			jnlfile_truncation = TRUE;
			jnl_file_close(jpc->region, TRUE, TRUE);
			jnlfile_truncation = FALSE;
			rel_crit(gv_cur_region);
		} else
		{
			assert(ctl->concat_prev);
			strcpy(temp_file_name, ctl->jnl_fn);
			strcat(temp_file_name, "_rolled_bak");
			temp_file_name_len = strlen(temp_file_name);
			if (RENAME_FAILED == rename_file_if_exists(temp_file_name, temp_file_name_len, &info_status,
                                rename_fn, &rename_len))
                                gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, ctl->jnl_fn_len, ctl->jnl_fn,
                                        rename_len, rename_fn, info_status);
			if (0 != RENAME(ctl->jnl_fn, temp_file_name))
			{
				save_errno = errno;
				errptr = (char *)STRERROR(save_errno);
				util_out_print(errptr, TRUE, save_errno);
				util_out_print("MUR-E-ERRRENAME : Failed to rename file !AD to !AD", TRUE,
						ctl->jnl_fn_len, ctl->jnl_fn, temp_file_name_len, temp_file_name);
			}
		}
	}
}
