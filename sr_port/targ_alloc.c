/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
#include "min_max.h"
#include "hashtab.h"
#include "hashtab_mname.h"
#include "gtmimagename.h"

GBLREF	gv_namehead		*gv_target_list;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	enum gtmImageTypes	image_type;

gv_namehead *targ_alloc(int keysize, mname_entry *gvent, gd_region *reg)
{
	gv_namehead	*gvt, *gvt1;
	int4		index;
	int4		partial_size;
	int4		gvn_size;
	sgmnt_addrs	*csa;
	ht_ent_mname	*tabent;
	boolean_t	gvt_hashtab_present, added;
#ifdef DEBUG
	ssize_t		first_rec_size, last_rec_size, clue_size;
#endif

	csa = ((NULL != reg) && reg->open) ? &FILE_INFO(reg)->s_addrs : NULL;
	gvt_hashtab_present = FALSE;
	if ((NULL != gvent) && (NULL != csa))
	{
		assert(keysize = csa->hdr->max_key_size);
		if (NULL != csa->gvt_hashtab)
		{	/* Check if incoming gvname is already part of the database file specific hashtable. If so,
			 * return gv_target that already exists here instead of mallocing something new.
			 */
			if (NULL != (tabent = lookup_hashtab_mname(csa->gvt_hashtab, gvent)))
			{
				gvt = (gv_namehead *)tabent->value;
				assert(NULL != gvt);
				DEBUG_ONLY(
					/* Ensure that this gvt is already present in the gv_target linked list */
					for (gvt1 = gv_target_list; NULL != gvt1; gvt1 = gvt1->next_gvnh)
					{
						if (gvt1 == gvt)
							break;
					}
					assert(NULL != gvt1);
				)
				gvt->regcnt++;
				assert(csa == gvt->gd_csa);
				return gvt;
			}
			gvt_hashtab_present = TRUE;
		}
	}
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
	/* If "reg" is non-NULL, but "gvent" is NULL, then it means the targ_alloc is being done for the directory tree.
	 * In that case, set gvt->root appropriately to DIR_ROOT. Else set it to 0. Also assert that the region is
	 * open in this case with the only exception being if called from mur_forward for non-invasive operations (e.g. EXTRACT).
	 */
	assert((NULL != gvent) || (NULL == reg) || reg->open || (MUPIP_IMAGE == image_type) && !mupip_jnl_recover);
	gvt->root = ((NULL != reg) && (NULL == gvent) ? DIR_ROOT : 0);
	gvt->nct = 0;
	gvt->act = 0;
	gvt->ver = 0;
	gvt->regcnt = 1;
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
	gvt->prev_gvnh = NULL;
	gvt->next_gvnh = gv_target_list;		/* Insert into gv_target list */
	if (NULL != gv_target_list)
		gv_target_list->prev_gvnh = gvt;
	gv_target_list = gvt;
	if (gvt_hashtab_present)
	{	/* Add new gvt to the database-file specific gvt hashtable */
		added = add_hashtab_mname(csa->gvt_hashtab, &gvt->gvname, gvt, &tabent);
		assert(added);
	}
	gvt->gd_csa = csa;
	return gvt;
}

void	targ_free(gv_namehead *gvt)
{
	gv_namehead	*prev_gvnh, *next_gvnh;

	assert(0 == gvt->regcnt);
	prev_gvnh = gvt->prev_gvnh;
	next_gvnh = gvt->next_gvnh;
	/* Input "gvt" can NOT be part of the gv_target_list (i.e. not allocated through targ_alloc) in case of a GT.CM GNP or
	 * DDP client. In that case though, prev_gvnh, next_gvnh and alt_hist all should be NULL. Assert that below.
	 */
	assert((NULL != prev_gvnh)
		|| ((NULL != gvt->alt_hist) && ((NULL != next_gvnh) || (gv_target_list == gvt)))/* WAS allocated by targ_alloc */
		|| ((NULL == gvt->alt_hist) && (NULL == next_gvnh) && (gv_target_list != gvt)));/* NOT allocated by targ_alloc */
	if (gvt == gv_target_list)
		gv_target_list = next_gvnh;
	else if (NULL != prev_gvnh)
		prev_gvnh->next_gvnh = next_gvnh;
	if (NULL != next_gvnh)
		next_gvnh->prev_gvnh = prev_gvnh;
	if (NULL != gvt->alt_hist)	/* can be NULL for GT.CM GNP or DDP client */
		free(gvt->alt_hist);
	free(gvt);
}
