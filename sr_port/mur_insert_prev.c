/****************************************************************
 *								*
 *	Copyright 2001-2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "min_max.h"
#include "gtm_string.h"

#ifdef VMS
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "hashtab.h"
#include "buddy_list.h"
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "gtmmsg.h"

GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	int		mur_regno;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	mur_opt_struct	mur_options;

boolean_t mur_insert_prev(void)
{
	reg_ctl_list	*rctl;
	jnl_ctl_list	*new_jctl, *cur_jctl, *jctl;
	redirect_list	*rl_ptr;
	boolean_t	proceed;

	error_def(ERR_DBJNLNOTMATCH);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_MUJNLPREVGEN);
	error_def(ERR_JNLTNOUTOFSEQ);
	error_def(ERR_JNLCYCLE);

	rctl = &mur_ctl[mur_regno];
	jctl = mur_jctl;
   	assert(rctl->jctl_head == jctl);
   	assert(rctl->jctl == jctl);
	new_jctl = (jnl_ctl_list *)malloc(sizeof(jnl_ctl_list));
	memset(new_jctl, 0, sizeof(jnl_ctl_list));
	memcpy(new_jctl->jnl_fn, jctl->jfh->prev_jnl_file_name, jctl->jfh->prev_jnl_file_name_length);
	new_jctl->jnl_fn_len = jctl->jfh->prev_jnl_file_name_length;
	assert(0 != new_jctl->jnl_fn_len);
	if (FALSE == mur_fopen(new_jctl))
	{
		free(new_jctl);
		return FALSE;	/* mur_fopen() would have printed the appropriate error message */
	}
	mur_jctl = new_jctl;	/* note: mur_fread_eof must be done after setting mur_ctl, mur_regno and mur_jctl */
	if (SS_NORMAL != (new_jctl->status = mur_fread_eof(new_jctl)))
	{
		gtm_putmsg(VARLSTCNT(6) ERR_JNLBADRECFMT, 3, new_jctl->jnl_fn_len, new_jctl->jnl_fn,
				new_jctl->rec_offset, new_jctl->status);
		mur_jctl = jctl;
		return FALSE;
	}
	assert(!mur_options.forward || (!(jctl->jfh->recover_interrupted && !new_jctl->jfh->recover_interrupted)));
	/* Skip the continuty of journal files check if both of these are true:
	 * 1) if current generation was created by recover and
	 * 2) the new one to be inserted was not created by recover
	 */
	if (!(jctl->jfh->recover_interrupted && !new_jctl->jfh->recover_interrupted))
	{
		if (!new_jctl->properly_closed)
		{
			proceed = (FALSE == mur_report_error(MUR_PREVJNLNOEOF));/* mur_report_error() will print error message */
			if (mur_options.update || !proceed)
			{
				mur_jctl = jctl;
				return FALSE;
			}
		}
		if ((!mur_options.forward || !mur_options.notncheck) && (new_jctl->jfh->eov_tn != jctl->jfh->bov_tn))
		{
			gtm_putmsg(VARLSTCNT(8) ERR_JNLTNOUTOFSEQ, 6,
				new_jctl->jfh->eov_tn, new_jctl->jnl_fn_len, new_jctl->jnl_fn,
				jctl->jfh->bov_tn, jctl->jnl_fn_len, jctl->jnl_fn);
			mur_jctl = jctl;
			return FALSE;
		}
	}
	if ((rctl->gd->dyn.addr->fname_len != new_jctl->jfh->data_file_name_length) ||
		(0 != memcmp(new_jctl->jfh->data_file_name, rctl->gd->dyn.addr->fname, rctl->gd->dyn.addr->fname_len)))
	{
		for (rl_ptr = mur_options.redirect;  (NULL != rl_ptr);  rl_ptr = rl_ptr->next)
		{
			if ((new_jctl->jfh->data_file_name_length == rl_ptr->org_name_len)
				&& (0 == memcmp(new_jctl->jfh->data_file_name,
					rl_ptr->org_name, rl_ptr->org_name_len)))
				break;
		}
		if (NULL == rl_ptr)
		{
			gtm_putmsg(VARLSTCNT(8) ERR_DBJNLNOTMATCH, 6, DB_LEN_STR(rctl->gd), new_jctl->jnl_fn_len,
					new_jctl->jnl_fn, new_jctl->jfh->data_file_name_length,
					new_jctl->jfh->data_file_name);
			mur_jctl = jctl;
			return FALSE;
		}
	}
	for (cur_jctl = rctl->jctl_head; cur_jctl; cur_jctl = cur_jctl->next_gen)
	{
		if (new_jctl->jfh->prev_jnl_file_name_length == cur_jctl->jnl_fn_len &&
			0 == memcmp(new_jctl->jfh->prev_jnl_file_name, cur_jctl->jnl_fn, cur_jctl->jnl_fn_len))
		{
			gtm_putmsg(VARLSTCNT(6) ERR_JNLCYCLE, 4, cur_jctl->jnl_fn_len, cur_jctl->jnl_fn, DB_LEN_STR(rctl->gd));
			mur_jctl = jctl;
			return FALSE;
		}
		if (new_jctl->jfh->turn_around_offset && cur_jctl->jfh->turn_around_offset)
			GTMASSERT; /* out of design situation */
	}
	new_jctl->prev_gen = NULL;
	new_jctl->next_gen = jctl;
	jctl->prev_gen = new_jctl;
	rctl->jctl = rctl->jctl_head = new_jctl;
	gtm_putmsg(VARLSTCNT(6) ERR_MUJNLPREVGEN, 4, new_jctl->jnl_fn_len, new_jctl->jnl_fn, DB_LEN_STR(mur_ctl[mur_regno].gd));
	return TRUE;
}
