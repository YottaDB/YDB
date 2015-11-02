/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#ifdef VMS
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gdsblk.h"
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
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmmsg.h"		/* for "gtm_putmsg" prototype */
#include "mur_read_file.h"	/* for "mur_fread_eof" prototype */

GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_gbls_t	murgbl;

boolean_t mur_jctl_from_next_gen(void)
{
	reg_ctl_list	*rctl, *rctl_top;
	jnl_ctl_list	*jctl, *temp_jctl;

	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_TEXT);

	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		if (!rctl->jfh_recov_interrupted)
			continue;
		assert(rctl->jctl_apply_pblk == rctl->jctl_head);
		jctl = rctl->jctl_apply_pblk;	/* journal file that has the turn around point of interrupted recovery */
		assert(rctl->jctl == jctl);
		assert(NULL != rctl->jctl_alt_head);	/* should have been set in mur_apply_pblk */
		assert(NULL != jctl->jfh);
		assert(!jctl->jfh->recover_interrupted);
		for (; NULL != jctl->next_gen; jctl = jctl->next_gen)
			assert(!jctl->next_gen->jfh->recover_interrupted);
		while (0 != jctl->jfh->next_jnl_file_name_length)
		{	/* create the linked list of journal files created by GT.M originally */
			temp_jctl = (jnl_ctl_list *)malloc(SIZEOF(jnl_ctl_list));
			memset(temp_jctl, 0, SIZEOF(jnl_ctl_list));
			temp_jctl->jnl_fn_len = jctl->jfh->next_jnl_file_name_length;
			memcpy(temp_jctl->jnl_fn, jctl->jfh->next_jnl_file_name, jctl->jfh->next_jnl_file_name_length);
			if (!mur_fopen(temp_jctl))
			{
				free(temp_jctl);
				return FALSE;
			}
			if (SS_NORMAL != (jctl->status = mur_fread_eof(temp_jctl, rctl)))
			{
				gtm_putmsg(VARLSTCNT(9) ERR_JNLBADRECFMT, 3,
					temp_jctl->jnl_fn_len, temp_jctl->jnl_fn, temp_jctl->rec_offset,
						ERR_TEXT, 2, LEN_AND_LIT("mur_jctl_from_next_gen"));
				free(temp_jctl);
				return FALSE;
			}
			temp_jctl->prev_gen = jctl;
			temp_jctl->next_gen = NULL;
			jctl->next_gen = temp_jctl;
			jctl = temp_jctl;
		}
		rctl->jctl = jctl;
		assert(jctl->reg_ctl == rctl);
	}
	return TRUE;
}
