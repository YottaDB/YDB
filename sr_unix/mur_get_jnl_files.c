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

#include <unistd.h>
#include "gtm_fcntl.h"
#include <errno.h>
#include "gtm_stat.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "cli.h"
#include "gtmio.h"
#include "iosp.h"
#include "copy.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "mupip_exit.h"
#include "dpgbldir.h"
#include "updproc.h"


error_def(ERR_FILENOTFND);
error_def(ERR_MUPCLIERR);
error_def(ERR_NORECOVERERR);
error_def(ERR_JNLALIGN);
error_def(ERR_JNLRECFMT);
error_def(ERR_JNLREADEOF);
error_def(ERR_JNLBADLABEL);


static sgmnt_data_ptr_t		csd;
ctl_list			*get_rollback_jnlfiles(void);
GBLREF	mur_opt_struct		mur_options;

ctl_list	*mur_get_jnl_files(void)
{
	ctl_list	head, *ctl = &head, *temp_ctl;
	struct stat	stat_buff;
	int		stat_res;
	unsigned short	retlen;
	char		buff[MAX_LINE], fn[MAX_FN_LEN + 1], *c, *c1, *ctop;


	retlen = sizeof(buff);

	if (!cli_get_str("FILE", buff, &retlen))
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);


	for (head.next = NULL, head.prev = NULL, c = c1 = buff, ctop = &buff[retlen];  c < ctop;  c1 = ++c)
	{
		while (*c != '\0'  &&  *c != ',' && *c != '"'  &&  *c != ' ')
			++c;

		memcpy(fn, c1, c - c1);
		fn[c - c1] = '\0';
		if (strcmp(fn ,"*") == 0)
		{
			if (mur_options.forward)
			{
				util_out_print("Star qualifier can not be specified with Forward recovery", TRUE);
				mupip_exit(ERR_NORECOVERERR);
			}
			else
			{
				ctl = get_rollback_jnlfiles();
				return ctl;
			}
		}
		STAT_FILE(fn, &stat_buff, stat_res);
		if (-1 == stat_res)
			if (errno == ENOENT)
				rts_error(VARLSTCNT(4) ERR_FILENOTFND, 2, c - c1, fn);
			else
				rts_error(VARLSTCNT(1) errno);

		temp_ctl = ctl;
		ctl = ctl->next
		    = (ctl_list *)malloc(sizeof(ctl_list));
		memset(ctl, 0, sizeof(ctl_list));
		ctl->prev = temp_ctl;
		ctl->jnl_fn_len = strlen(fn);
		strcpy(ctl->jnl_fn, fn);
	}

	if (head.next == NULL)
	{
			util_out_print("No journal files specified", TRUE);
			mupip_exit(ERR_NORECOVERERR);
	}

	head.next->prev = NULL;
	return head.next;
}

ctl_list *get_rollback_jnlfiles(void)
{
	mval 		v;
	upd_proc_ctl	*rollbk_db_files, *curr;
	int		i, db_fd, fn_len;
	int4		status, cl_status;
	char		fn[MAX_FN_LEN];
	ctl_list	head, *ctl = &head, *temp_ctl, *back_gen_files, *temp_prev;
	jnl_file_header	*cp;
	off_t		n;
	int		fd;
	gd_addr		*temp_gd_header;


	v.mvtype = MV_STR;
	v.str.len = 0;
	temp_gd_header = zgbldir(&v);
	rollbk_db_files = read_db_files_from_gld(temp_gd_header);

	head.next = NULL;
	head.prev = NULL;

	csd = (sgmnt_data_ptr_t)malloc(ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE));

	for (curr = rollbk_db_files; curr != NULL; curr = curr->next)
	{
		fn_len = curr->gd->dyn.addr->fname_len;
		memcpy(fn, curr->gd->dyn.addr->fname, fn_len);
		fn[fn_len] = '\0';

		curr->db_ctl = (file_control *)malloc(sizeof(file_control));
		memset(curr->db_ctl, 0, sizeof(file_control));

		OPENFILE(fn, O_RDWR, db_fd);
		if (-1 == db_fd)
		{
			util_out_print("Error opening database file !AZ - status : ", TRUE, fn);
			mur_output_status(errno);
			return FALSE;
		}

		LSEEKREAD(db_fd, 0, csd, sizeof(sgmnt_data), status);
		CLOSEFILE(db_fd, cl_status);
		if (-1 == cl_status)
		{
			cl_status = errno;
			util_out_print("Error closing database file !AZ - cl_status : ", TRUE, fn);
			mur_output_status(cl_status);
		}

		if (0 != status)
		{
			if (-1 == status)
				util_out_print("Premature end-of-file for database file !AZ", TRUE, fn);
			else
			{
				util_out_print("Error opening database file !AZ - status : ", TRUE, fn);
				mur_output_status(status);
			}
			return FALSE;
		}
		assert(!mur_options.forward);
		if (mur_options.rollback && !JNL_ENABLED(csd))
			continue;
		else if (!mur_options.rollback && mur_options.update && !JNL_ALLOWED(csd))
			continue;
		temp_ctl = ctl;
		ctl = ctl->next
	   	    = (ctl_list *)malloc(sizeof(ctl_list));
		memset(ctl, 0, sizeof(ctl_list));
		ctl->prev = temp_ctl;
		ctl->jnl_fn_len = csd->jnl_file_len;
		strcpy(ctl->jnl_fn, csd->jnl_file_name);
	}
	free (csd);

	if (NULL != head.next)
		head.next->prev = NULL;
	return head.next;
}

