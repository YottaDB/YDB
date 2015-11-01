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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"

GBLREF gv_namehead	*gv_target_list;

gv_namehead *targ_alloc(int keysize)
{
	gv_namehead	*gvt;
	int4		index;

	keysize = (keysize + MAX_NUM_SUBSC_LEN + 4) & (-4);

	gvt = (gv_namehead *)malloc(sizeof(gv_namehead) + 2 * sizeof(gv_key) + 3 * (keysize - 1));
	gvt->first_rec = (gv_key *)(gvt->clue.base + keysize);
	gvt->last_rec = (gv_key *)(gvt->first_rec->base + keysize);
	gvt->clue.top = gvt->last_rec->top = gvt->first_rec->top = keysize;
	gvt->clue.prev = gvt->clue.end = 0;
	gvt->root = 0;
	gvt->nct = 0;
	gvt->act = 0;
	gvt->ver = 0;
	gvt->collseq = NULL;
	gvt->read_local_tn = (trans_num)0;
	gvt->write_local_tn = (trans_num)0;
	gvt->noisolation = FALSE;
	gvt->alt_hist = (srch_hist *)malloc(sizeof(srch_hist));
	for (index = 0; index < MAX_BT_DEPTH; index++)
	{
		gvt->hist.h[index].level = index;	/* needed in TP to know what level a history element is */
		gvt->hist.h[index].blk_target = gvt;
		gvt->alt_hist->h[index].level = index;
		gvt->alt_hist->h[index].blk_target = gvt;
	}
	gvt->next_gvnh = gv_target_list;		/* Insert into gv_target list */
	gv_target_list = gvt;
	return gvt;
}
