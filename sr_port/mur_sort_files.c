/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
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
	reg_ctl_list	temp;

	reg_total = murgbl.reg_total;
	for (reg_total -= 2; reg_total >= 0; reg_total--)
	{
		for (regno = 0; regno <= reg_total; regno++)
		{
			if (mur_ctl[regno].lvrec_time > mur_ctl[regno + 1].lvrec_time)
			{
				temp = mur_ctl[regno];
				mur_ctl[regno] = mur_ctl[regno + 1];
				mur_ctl[regno + 1] = temp;
			}
		}
	}

}
