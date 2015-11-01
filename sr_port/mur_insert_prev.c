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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "iosp.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	int		mur_extract_bsize;
GBLREF	char		*log_rollback;


bool	mur_insert_prev(ctl_list *ctl, ctl_list **jnl_files)
{
	ctl_list	*curr;
	jnl_file_header	*header, *prev_header;
	int		status;

	header = mur_get_file_header(ctl->rab);
	if (0 == header->prev_jnl_file_name_length)
         	return mur_report_error(ctl, MUR_MISSING_PREVLINK);
	curr = (ctl_list *)malloc(sizeof(ctl_list));
	memset(curr, 0, sizeof(ctl_list));
	curr->next = ctl;
	curr->prev = ctl->prev;
	if (NULL != ctl->prev)
	{
		assert(FALSE == ctl->concat_prev);
		assert(FALSE == ctl->prev->concat_next);
		ctl->prev->next = curr;
	} else
	{
		assert(*jnl_files == ctl);
		*jnl_files = curr;
	}
	ctl->prev = curr;
	ctl->concat_prev = TRUE;
	curr->concat_next = TRUE;
	curr->concat_prev = FALSE;
	if (log_rollback)
	{
		util_out_print("MUR-I-JNLPREVGEN: Database File  ---->  !AD", TRUE,
					header->data_file_name_length, header->data_file_name);
		util_out_print("                  Current Generation Jnl File  ---->  !AD", TRUE, ctl->jnl_fn_len, ctl->jnl_fn);
		util_out_print("                  Previous Generation Jnl File  ---->  !AD", TRUE,
								header->prev_jnl_file_name_length, header->prev_jnl_file_name);
	}
	curr->jnl_fn_len = header->prev_jnl_file_name_length;
	memcpy(curr->jnl_fn, header->prev_jnl_file_name, curr->jnl_fn_len);
	curr->gd = ctl->gd;
	curr->db_ctl = ctl->db_ctl;
	curr->db_tn = ctl->db_tn;
	curr->turn_around_tn = ctl->turn_around_tn;
	curr->jnl_tn = ctl->jnl_tn;
	curr->jnl_state = ctl->jnl_state;
	curr->repl_state = ctl->repl_state;
	curr->tab_ptr = ctl->tab_ptr;
	curr->rab = mur_rab_create(MINIMUM_BUFFER_SIZE);
	/* When we are processing previous generation file, database should have before image journaled
	 * Assign before image flag of previous generation to be the same as latest generation opened */
	curr->before_image = ctl->before_image;
	/* Initiate hashtable for the previous generation also, used in pini_addr tracking */
	init_hashtab(&curr->pini_in_use, MUR_PINI_IN_USE_INIT_ELEMS);
	if (SS_NORMAL != (status = mur_fopen(curr->rab, curr->jnl_fn, curr->jnl_fn_len)))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		VMS_ONLY(mur_open_files_error(curr);)
		UNIX_ONLY(mur_open_files_error(curr, NO_FD_OPEN);)
		return FALSE;
	}
	if (SS_NORMAL != (status = mur_fread_eof(curr->rab, curr->jnl_fn, curr->jnl_fn_len)))
	{
		gtm_putmsg(VARLSTCNT(1) status);
		VMS_ONLY(mur_open_files_error(curr);)
		UNIX_ONLY(mur_open_files_error(curr, NO_FD_OPEN);)
		return FALSE;
	}
	prev_header = mur_get_file_header(curr->rab);
	COPY_BOV_FIELDS_FROM_JNLHDR_TO_CTL(prev_header, curr);
	if ((FALSE == mur_jnlhdr_bov_check(prev_header, curr->jnl_fn_len, curr->jnl_fn))
			|| (FALSE == mur_jnlhdr_multi_bov_check(prev_header, curr->jnl_fn_len, curr->jnl_fn,
								header, curr->next->jnl_fn_len, curr->next->jnl_fn, TRUE)))
		return FALSE;
	header = prev_header;
	if (!mur_options.forward  &&  !header->before_images)
	{
		util_out_print("Journal file !AD does not contain before-images;  cannot do backward recovery", TRUE,
				curr->jnl_fn_len, curr->jnl_fn);
		VMS_ONLY(mur_open_files_error(curr);)
		UNIX_ONLY(mur_open_files_error(curr, NO_FD_OPEN);)
		return FALSE;
	}
	if (NULL != mur_options.losttrans_file_info)
	{
		if (header->max_record_length > mur_extract_bsize)
			mur_extract_bsize = header->max_record_length;
		if (0 == mur_extract_bsize)
			mur_extract_bsize = DEFAULT_EXTR_BLKSIZE;
	}
	if (NULL != mur_options.extr_file_info)
	{
		if (header->max_record_length > mur_extract_bsize)
			mur_extract_bsize = header->max_record_length;
		if (0 == mur_extract_bsize)
			mur_extract_bsize = DEFAULT_EXTR_BLKSIZE;
	}
	return TRUE;
}
