/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;

error_def(ERR_DBJNLNOTMATCH);
error_def(ERR_JNLBADRECFMT);
error_def(ERR_JNLCYCLE);
error_def(ERR_JNLTNOUTOFSEQ);
error_def(ERR_MUJNLPREVGEN);


boolean_t mur_insert_prev(jnl_ctl_list **jjctl)
{
	reg_ctl_list	*rctl;
	jnl_ctl_list	*new_jctl, *cur_jctl, *jctl;
	redirect_list	*rl_ptr;
	boolean_t	proceed;

	jctl = *jjctl;
	rctl = jctl->reg_ctl;
   	assert(rctl->jctl_head == jctl);
   	assert(rctl->jctl == jctl);
	new_jctl = (jnl_ctl_list *)malloc(SIZEOF(jnl_ctl_list));
	memset(new_jctl, 0, SIZEOF(jnl_ctl_list));
	memcpy(new_jctl->jnl_fn, jctl->jfh->prev_jnl_file_name, jctl->jfh->prev_jnl_file_name_length);
	new_jctl->jnl_fn_len = jctl->jfh->prev_jnl_file_name_length;
	assert(0 != new_jctl->jnl_fn_len);
	if (FALSE == mur_fopen(new_jctl))
	{
		free(new_jctl);
		return FALSE;	/* "mur_fopen" would have printed the appropriate error message */
	}
	if (SS_NORMAL != (new_jctl->status = mur_fread_eof(new_jctl, rctl)))
	{
		gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_JNLBADRECFMT, 3, new_jctl->jnl_fn_len,
			new_jctl->jnl_fn, new_jctl->rec_offset, new_jctl->status);
		free(new_jctl);
		return FALSE;
	}
	/* It is NOT possible for a previous generation journal file to have jfh->crash to be TRUE. Assert that.
	 * This also indicates that jctl->properly_closed will always be TRUE for a previous generation journal
	 * file. The only exception is if the current generation journal file is created by recovery. In this
	 * case the previous generation journal file (not created by recover) can have the crash field set to TRUE.
	 */
	assert((new_jctl->properly_closed && !new_jctl->jfh->crash) || (jctl->jfh->recover_interrupted &&
			!new_jctl->jfh->recover_interrupted));
	assert(!mur_options.forward || (!(jctl->jfh->recover_interrupted && !new_jctl->jfh->recover_interrupted)));
	/* Skip the continuity of journal files check if both of these are true:
	 * 1) if current generation was created by recover and
	 * 2) the new one to be inserted was not created by recover
	 */
	if (!(jctl->jfh->recover_interrupted && !new_jctl->jfh->recover_interrupted))
	{
		if (!new_jctl->properly_closed)
		{
			proceed = (FALSE == mur_report_error(jctl, MUR_PREVJNLNOEOF)); /* message report left to mur_report_error */
			if (mur_options.update || !proceed)
			{
				free(new_jctl);
				return FALSE;
			}
		}
		if ((!mur_options.forward || !mur_options.notncheck) && (new_jctl->jfh->eov_tn != jctl->jfh->bov_tn))
		{
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(8) ERR_JNLTNOUTOFSEQ, 6,
				&new_jctl->jfh->eov_tn, new_jctl->jnl_fn_len, new_jctl->jnl_fn,
				&jctl->jfh->bov_tn, jctl->jnl_fn_len, jctl->jnl_fn);
			free(new_jctl);
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
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(8) ERR_DBJNLNOTMATCH,
				6, DB_LEN_STR(rctl->gd), new_jctl->jnl_fn_len, new_jctl->jnl_fn,
				new_jctl->jfh->data_file_name_length, new_jctl->jfh->data_file_name);
			free(new_jctl);
			return FALSE;
		}
	}
	for (cur_jctl = rctl->jctl_head; cur_jctl; cur_jctl = cur_jctl->next_gen)
	{
		if (new_jctl->jfh->prev_jnl_file_name_length == cur_jctl->jnl_fn_len &&
			0 == memcmp(new_jctl->jfh->prev_jnl_file_name, cur_jctl->jnl_fn, cur_jctl->jnl_fn_len))
		{
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_JNLCYCLE, 4, cur_jctl->jnl_fn_len,
				cur_jctl->jnl_fn, DB_LEN_STR(rctl->gd));
			free(new_jctl);
			return FALSE;
		}
		if (new_jctl->jfh->turn_around_offset && cur_jctl->jfh->turn_around_offset)
		{
			if (rctl->recov_interrupted)
			{	/* Possible if a first recovery with a turn-around-point (T2) got interrupted and a second
				 * recovery with a new turn-around-point (T1 which is in a previous generation journal file)
				 * was re-interrupted while in the middle of mur_process_intrpt_recov just after it had
				 * recorded the new turn-around-point (T1) but before it had erased the former one (T2).
				 * In this case, erase the turn-around-point T2 so this recovery goes back to T1. Here we
				 * erase the value only in memory. The value on disk is reset later in mur_process_intrpt_recov.
				 */
				cur_jctl->jfh->turn_around_offset = 0;
				cur_jctl->jfh->turn_around_time = 0;
			} else
				GTMASSERT; /* out of design situation */
		}
	}
	new_jctl->prev_gen = NULL;
	new_jctl->next_gen = jctl;
	jctl->prev_gen = new_jctl;
	rctl->jctl = rctl->jctl_head = new_jctl;
	assert(new_jctl->reg_ctl == rctl);
	*jjctl = new_jctl;
	gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(6) ERR_MUJNLPREVGEN, 4, new_jctl->jnl_fn_len,
		new_jctl->jnl_fn, DB_LEN_STR(rctl->gd));
	return TRUE;
}
