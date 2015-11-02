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

#include "stddef.h"

#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "sbs_blk.h"
#include "mdq.h"
#include "gtm_malloc.h"

#define SBS_SUPER_BLK_SIZE	(6 * 1024)	/* Base size is 6K */

/* Malloc sbs blocks in chunks so don't fragement memory */

GBLDEF sbs_blk	*sbs_blk_hdr = NULL;

GBLREF int4	lv_sbs_blk_size;
GBLREF int4	lv_sbs_blk_scale;

sbs_blk *lv_get_sbs_blk (symval *sym)
{
	sbs_blk		*temp;
	unsigned char	*temp1;
	int		i, sbs_elem_cnt, tot_alloc;

	if (sbs_blk_hdr)
       	{
		temp = sbs_blk_hdr;
		sbs_blk_hdr = sbs_blk_hdr->sbs_que.fl;
	} else
       	{
		tot_alloc = SBS_SUPER_BLK_SIZE * lv_sbs_blk_scale;	/* Scale up size as necessary */
		sbs_elem_cnt = tot_alloc / lv_sbs_blk_size;
		tot_alloc = sbs_elem_cnt * lv_sbs_blk_size;	/* Recalc tot_alloc so exactly what we need - no waste */
		temp = (sbs_blk *)malloc(tot_alloc);
		for (temp1 = (unsigned char *)temp , i = 1; i < sbs_elem_cnt; i++)
		{
			temp1 += lv_sbs_blk_size;	/* Note skips the first sbs_blk which we are going to return */
			((sbs_blk *)temp1)->sbs_que.fl = sbs_blk_hdr;
			sbs_blk_hdr = (sbs_blk *)temp1;
		}
	}
	temp->cnt = 0;
	temp->nxt = 0;
	/* In the following macro, sym is implicitly treated as a (sbs_blk *). */
	dqins ((sbs_blk *)sym, sbs_que, temp);
	return (temp);
}
