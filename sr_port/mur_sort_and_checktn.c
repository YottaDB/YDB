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
#include "util.h"

GBLREF	mur_opt_struct	mur_options;


bool	mur_sort_and_checktn(ctl_list **jnl_files)
{
	ctl_list		*curr, *last_jnl_file;
	trans_num		check_tn;
	jnl_file_header		*header;

	/* Sort the list of journal files, if necessary */
	if (NULL != (*jnl_files)->next) 	/* There are at least two journal files;  sort them */
	{
		if (!mur_sort_files(jnl_files))
			return FALSE;
	} else
	{	/* fill in the bov_tn, eov_tn, bov_timestamp and eov_timestamp fields */
		header = mur_get_file_header((*jnl_files)->rab);
		(*jnl_files)->bov_tn = header->bov_tn;
		(*jnl_files)->eov_tn = header->eov_tn;
		(*jnl_files)->bov_timestamp = header->bov_timestamp;
		(*jnl_files)->eov_timestamp = header->eov_timestamp;
	}
	for (last_jnl_file = *jnl_files;  NULL != last_jnl_file->next;  last_jnl_file = last_jnl_file->next)
		;
	if (mur_options.update && mur_options.forward && !mur_options.notncheck)
	{
		for (curr = last_jnl_file;  NULL != curr; )
		{
			if (curr->concat_prev)
				check_tn = curr->prev->eov_tn;
			else
			{
				assert(curr->gd->open);
				check_tn = FILE_INFO(curr->gd)->s_addrs.hdr->trans_hist.curr_tn;
			}
			if (check_tn != curr->bov_tn)
			{
				if ((check_tn < curr->bov_tn) && (!curr->concat_prev) && mur_options.chain)
				{
					if (!mur_insert_prev(curr, jnl_files))
						return FALSE;
					continue;	/* make this curr go through the loop once more */
				}
				util_out_print("First Record in Journal file !AD has Transaction Number [!UL] ",
									TRUE, curr->jnl_fn_len, curr->jnl_fn, curr->bov_tn);
				if (!curr->concat_prev)
					util_out_print("  and Database file has different Transaction Number [!UL] - ",
													TRUE, check_tn);
				else
				{
					util_out_print("  and Last Record in Previous Journal File !AD has different Transaction "
							"Number [!UL]", TRUE, curr->prev->jnl_fn_len, curr->prev->jnl_fn, check_tn);
					util_out_print("The above two journal files are discontinuous", TRUE);
				}
				assert(FALSE);
				return FALSE;
			}
			curr = curr->prev;
		}
	}
	return TRUE;
}
