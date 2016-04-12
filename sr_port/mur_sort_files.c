/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"	/* For muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	reg_ctl_list	*mur_ctl;

/* This sorts mur_ctl[].
 * mur_ctl[x].lvrec_time is the last valid record's timestamp of journal file mur_ctl[x].jctl->jnl_fn.
 * Sort mur_ctl[] so that:
 * 	for x = 0 to (reg_total-2) we have :
 *		mur_ctl[x].lvrec_time <= mur_ctl[x+1].lvrec_time
 * Note: Bubble sort is okay, since only a few elements are present. Use qsort() in case of large no. of regions
 */
void	mur_sort_files(void)
{
	int 		reg_total, regno;
	reg_ctl_list	temp, *rctl0, *rctl1;
	jnl_ctl_list	*jctl;

	reg_total = murgbl.reg_total;
	for (reg_total -= 2; reg_total >= 0; reg_total--)
	{
		for (regno = 0; regno <= reg_total; regno++)
		{
			rctl0 = &mur_ctl[regno];
			rctl1 = rctl0 + 1;
			if (rctl0->lvrec_time > rctl1->lvrec_time)
			{
				temp = *rctl0;
				*rctl0 = *rctl1;
				*rctl1 = temp;
				/* Fix back pointers of jctl_head and jctl_alt_head lists in rctl0 */
				jctl = rctl0->jctl_head;
				MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(jctl, rctl0, rctl1, TRUE);
				/* When fixing jctl back pointers for jctl_alt_head, do not check prev_gen being NULL.
				 * This is because although it is non-NULL, that marks the beginning of the regular
				 * rctl->jctl_head linked list so we dont need to do any more special processing going
				 * backwards (only need to go forward from jctl_alt_head).
				 * Hence FALSE passed as the 4th parameter to MUR_FIX_JCTL_BACK_POINTER_TO_RCTL below
				 */
				jctl = rctl0->jctl_alt_head;
				if (NULL != jctl)
					MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(jctl, rctl0, rctl1, FALSE);
				/* Fix back pointers of jctl_head and jctl_alt_head lists in rctl1 */
				jctl = rctl1->jctl_head;
				MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(jctl, rctl1, rctl0, TRUE);
				jctl = rctl1->jctl_alt_head;
				if (NULL != jctl)
					MUR_FIX_JCTL_BACK_POINTER_TO_RCTL(jctl, rctl1, rctl0, FALSE);
			}
		}
	}

}
