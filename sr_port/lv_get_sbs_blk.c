/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "stddef.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "mdq.h"
#include "gtm_malloc.h"

/* Malloc sbs blocks in chunks so don't fragement memory
	malloc works on multiples of 1k so appoximate a
	multiple as closely as possible.
*/

#define SBS_MALLOC_SIZE (((6 * 1024) - offsetof(storElem, userStorage)) / SIZEOF(sbs_blk))

GBLDEF sbs_blk	*sbs_blk_hdr = 0;

sbs_blk *lv_get_sbs_blk (symval *sym)
{
	sbs_blk	*temp, *temp1;
	int	i;

	if (sbs_blk_hdr)
       	{
		temp = sbs_blk_hdr;
		sbs_blk_hdr = sbs_blk_hdr->sbs_que.fl;
	} else
       	{
		temp = (sbs_blk *)malloc(SIZEOF(sbs_blk) * SBS_MALLOC_SIZE);
		for (temp1 = temp , i = 1; i < SBS_MALLOC_SIZE; i++)
		{
			temp1++;
			temp1->sbs_que.fl = sbs_blk_hdr;
			sbs_blk_hdr = temp1;
		}
	}
	temp->cnt = 0;
	temp->nxt = 0;
	/* In the following macro, sym is implicitly treated as a (sbs_blk *). */
	dqins ((sbs_blk *)sym, sbs_que, temp);
	return (temp);
}
