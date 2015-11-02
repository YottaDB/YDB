/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"

GBLREF gv_namehead	*gv_target_list;

gv_namehead *targ_alloc(int keysize, mname_entry *gvent)
{
	gv_namehead	*gvt;
	int4		index;
	int4		partial_size;
	int4		gvn_size;
#ifdef DEBUG
	ssize_t		first_rec_size, last_rec_size, clue_size;
#endif
	keysize = ROUND_UP2((keysize + MAX_NUM_SUBSC_LEN), 4);	/* Alignment is done so that first_rec
							    	   and last_rec starts at aligned boundary */
	partial_size = SIZEOF(gv_namehead) + 2 * SIZEOF(gv_key) + 3 * keysize;
	gvn_size = (NULL == gvent) ? MAX_MIDENT_LEN : gvent->var_name.len;
	gvt = (gv_namehead *)malloc(partial_size + gvn_size);
	gvt->gvname.var_name.addr = (char *)gvt + partial_size;
	if (NULL != gvent)
	{
		memcpy(gvt->gvname.var_name.addr, gvent->var_name.addr, gvent->var_name.len);
		gvt->gvname.var_name.len = gvent->var_name.len;
		gvt->gvname.hash_code = gvent->hash_code;
	} else
	{
		gvt->gvname.var_name.len = 0;
		gvt->gvname.hash_code = 0;
	}
	gvt->first_rec = (gv_key *)((char *)&gvt->clue + sizeof(gv_key) + keysize);
	gvt->last_rec = (gv_key *)((char *)gvt->first_rec + sizeof(gv_key) + keysize);
	assert((UINTPTR_T)gvt->first_rec % sizeof(gvt->first_rec->top) == 0);
	assert((UINTPTR_T)gvt->last_rec % sizeof(gvt->last_rec->top) == 0);
	assert((UINTPTR_T)gvt->first_rec % sizeof(gvt->first_rec->end) == 0);
	assert((UINTPTR_T)gvt->last_rec % sizeof(gvt->last_rec->end) == 0);
	assert((UINTPTR_T)gvt->first_rec % sizeof(gvt->first_rec->prev) == 0);
	assert((UINTPTR_T)gvt->last_rec % sizeof(gvt->last_rec->prev) == 0);
	DEBUG_ONLY(clue_size = (char *)gvt->first_rec - (char *)&gvt->clue);
	DEBUG_ONLY(first_rec_size = (char *)gvt->last_rec - (char *)gvt->first_rec);
	DEBUG_ONLY(last_rec_size = (char *)gvt->gvname.var_name.addr - (char *)gvt->last_rec);
	assert(clue_size == first_rec_size);
	assert(clue_size == last_rec_size);
	assert(clue_size == (sizeof(gv_key) + keysize));
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
	/* Initialize the 0:MAX_BT_DEPTH. Otherwise, memove of the array in mu_reorg can cause problem */
	for (index = 0; index <= MAX_BT_DEPTH; index++)
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
