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

GBLREF	mur_opt_struct	mur_options;
GBLREF	seq_num		consist_jnl_seqno;
GBLREF	jnl_proc_time	min_jnl_rec_time;

error_def(ERR_JNLNOTFOUND);

bool mur_check_jnlfiles_present(ctl_list **jnl_files)
{
	ctl_list	*curr;
	jnl_file_header	*header, *prev_header;
	struct stat	stat_buf;
	int		stat_res, fd, status, prev_fn_len, fn_len, hdr_len;
	char		prev_fn[MAX_FN_LEN], fn[MAX_FN_LEN];
	char		*hdr_buffer, *prev_hdr_buffer;
	unsigned char	*ptr, qwstring[100], *ptr1, qwstring1[100];

	hdr_len = ROUND_UP(sizeof(jnl_file_header), sizeof(uint4) *2);
	hdr_buffer = (char *)malloc(hdr_len);
	prev_hdr_buffer = (char *)malloc(hdr_len);
	for (curr = *jnl_files; curr != NULL; curr = curr->next)
	{
		fn_len = curr->jnl_fn_len;
		memcpy(fn, curr->jnl_fn, fn_len);
		fn[fn_len] = '\0';
		LSEEKREAD(curr->rab->pvt->fd, 0, (sm_uc_ptr_t)hdr_buffer, hdr_len, status);
		if (curr->concat_prev)
			continue;
		header = (jnl_file_header *)hdr_buffer;
		if (!mur_options.rollback)
		{
			if (header->bov_timestamp < MID_TIME(min_jnl_rec_time))
				continue;
		} else if (QWLE(header->start_seqno, consist_jnl_seqno))
			continue;
		while(0 != header->prev_jnl_file_name_length)
		{
			prev_fn_len = header->prev_jnl_file_name_length;
			memcpy(prev_fn, header->prev_jnl_file_name, prev_fn_len);
			prev_fn[prev_fn_len] = '\0';
			STAT_FILE(prev_fn, &stat_buf, stat_res);
			if (-1 != stat_res)
			{
				OPENFILE((sm_c_ptr_t)prev_fn, mur_options.rollback ? O_RDWR:O_RDONLY, fd); /* RDWR for rollbk */
				if (-1 == fd)
				{
					free(hdr_buffer);
					free(prev_hdr_buffer);
					util_out_print("Error opening Journal file !AZ - status:  ", TRUE, prev_fn);
					mur_open_files_error(curr, fd);
					return FALSE;
				}
				LSEEKREAD(fd, 0, (sm_uc_ptr_t)prev_hdr_buffer, hdr_len, status);
				if (0 != status)
				{
					free(hdr_buffer);
					free(prev_hdr_buffer);
					mur_open_files_error(curr, fd);
					return FALSE;
				}
				CLOSEFILE(fd, status);
				if (-1 == status)
				{
					status = errno;
					util_out_print("Error closing Journal file !AZ - status : ", TRUE, prev_fn);
					mur_output_status(status);
				}
				prev_header = (jnl_file_header *)prev_hdr_buffer;
				if ((FALSE == mur_jnlhdr_bov_check(prev_header, prev_fn_len, prev_fn))
						|| (FALSE == mur_jnlhdr_multi_bov_check(prev_header, prev_fn_len, prev_fn,
												header, fn_len, fn, TRUE)))
				{
					free(hdr_buffer);
					free(prev_hdr_buffer);
					mur_open_files_error(curr, fd);
					return FALSE;
				}
				memcpy(header, prev_header, hdr_len);
				if (mur_options.rollback && QWLE(header->start_seqno, consist_jnl_seqno))
					break;
				else if (!mur_options.rollback && header->bov_timestamp < mur_options.lookback_time)
					break;
				else
					continue;
			} else /* Error file doesn't exist on the system */
			{
				if (mur_options.rollback)
				{
					ptr = i2ascl(qwstring, header->start_seqno);
					ptr1 = i2asclx(qwstring1, header->start_seqno);
					util_out_print("Journal file !AZ -> Start Seqno = !AD [0x!AD]is greater than Seqno",
							FALSE, fn, ptr-qwstring, qwstring, ptr1 - qwstring1, qwstring1);
					util_out_print("to be rolled back - Rollback not possible ", TRUE);
					gtm_putmsg(VARLSTCNT(4) ERR_JNLNOTFOUND, 2, prev_fn_len, prev_fn);
				} else
				{
					util_out_print("Journal file !AZ -> Start Timestamp = !UL is greater than Lookback",
							FALSE, fn, header->bov_timestamp);
					util_out_print("Time specified - Recovery not possible ", TRUE);
					gtm_putmsg(VARLSTCNT(4) ERR_JNLNOTFOUND, 2, prev_fn_len, prev_fn);
				}
				free(hdr_buffer);
				free(prev_hdr_buffer);
				return FALSE;
			}
			fn_len = prev_fn_len;
			memcpy(fn, prev_fn, fn_len);
			fn[fn_len] = '\0';
		}
		if (mur_options.rollback && QWGT(header->start_seqno, consist_jnl_seqno))
		{
			assert (0 == header->prev_jnl_file_name_length);
			ptr = i2ascl(qwstring, header->start_seqno);
			ptr1 = i2asclx(qwstring1, header->start_seqno);
			util_out_print("Region !AZ --> Start Seqno = !AD [0x!AD] is greater than Seqno ",
						FALSE, fn, ptr-qwstring, qwstring, ptr1 - qwstring1, qwstring1);
			util_out_print("to be rolled - Rollback not possible ", TRUE);
			free(hdr_buffer);
			free(prev_hdr_buffer);
			return mur_report_error(curr, MUR_MISSING_PREVLINK);
		}
	}
	free(hdr_buffer);
	free(prev_hdr_buffer);
	return TRUE;
}
