/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>		/* for offsetof macro (used by OFFSETOF macro) */

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "targ_alloc.h"
#include "min_max.h"
#include "hashtab_mname.h"
#include "gtmimagename.h"

GBLREF	gv_namehead		*gv_target_list;
GBLREF	gv_namehead		*gv_target;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	int			process_exiting;

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

	/* Ensure there are no additional compiler introduced filler bytes. This is a safety since a few elements in the
	 * gv_namehead structure are defined using MAX_BT_DEPTH macros and we want to guard against changes to this macro
	 * that cause unintended changes to the layout/size of the gv_namehead structure.
	 */
	assert(OFFSETOF(gv_namehead, filler_8byte_align1[0]) + SIZEOF(gvt->filler_8byte_align1)
			== OFFSETOF(gv_namehead, last_split_blk_num[0]));
#	ifdef GTM_TRIGGER
	assert(OFFSETOF(gv_namehead, last_split_blk_num[0]) + SIZEOF(gvt->last_split_blk_num)
			== OFFSETOF(gv_namehead, gvt_trigger));
	GTM64_ONLY(assert(OFFSETOF(gv_namehead, filler_8byte_align2) + SIZEOF(gvt->filler_8byte_align2)
			== OFFSETOF(gv_namehead, clue));)
	NON_GTM64_ONLY(assert(OFFSETOF(gv_namehead, trig_mismatch_test_done) + SIZEOF(gvt->trig_mismatch_test_done)
			== OFFSETOF(gv_namehead, clue));)
#	endif
	csa = ((NULL != reg) && reg->open) ? &FILE_INFO(reg)->s_addrs : NULL;
	gvt_hashtab_present = FALSE;
	if ((NULL != gvent) && (NULL != csa))
	{
		assert(keysize == csa->hdr->max_key_size);
		if (NULL != csa->gvt_hashtab)
		{	/* Check if incoming gvname is already part of the database file specific hashtable. If so,
			 * return gv_target that already exists here instead of mallocing something new.
			 */
			if (NULL != (tabent = lookup_hashtab_mname(csa->gvt_hashtab, gvent)))
			{
				gvt = (gv_namehead *)tabent->value;
				assert(NULL != gvt);
				/* Ensure that this gvt is already present in the gv_target linked list */
				DBG_CHECK_GVT_IN_GVTARGETLIST(gvt);
				gvt->regcnt++;
				assert(csa == gvt->gd_csa);
				return gvt;
			}
			gvt_hashtab_present = TRUE;
		}
	}
	keysize = DBKEYSIZE(keysize);
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
	assert(0 == (OFFSETOF(gv_namehead, clue) % SIZEOF(gvt->clue)));
	gvt->first_rec = (gv_key *)((char *)&gvt->clue + SIZEOF(gv_key) + keysize);
	gvt->last_rec = (gv_key *)((char *)gvt->first_rec + SIZEOF(gv_key) + keysize);
	assert((UINTPTR_T)gvt->first_rec % SIZEOF(gvt->first_rec->top) == 0);
	assert((UINTPTR_T)gvt->last_rec % SIZEOF(gvt->last_rec->top) == 0);
	assert((UINTPTR_T)gvt->first_rec % SIZEOF(gvt->first_rec->end) == 0);
	assert((UINTPTR_T)gvt->last_rec % SIZEOF(gvt->last_rec->end) == 0);
	assert((UINTPTR_T)gvt->first_rec % SIZEOF(gvt->first_rec->prev) == 0);
	assert((UINTPTR_T)gvt->last_rec % SIZEOF(gvt->last_rec->prev) == 0);
	DEBUG_ONLY(clue_size = (char *)gvt->first_rec - (char *)&gvt->clue);
	DEBUG_ONLY(first_rec_size = (char *)gvt->last_rec - (char *)gvt->first_rec);
	DEBUG_ONLY(last_rec_size = (char *)gvt->gvname.var_name.addr - (char *)gvt->last_rec);
	assert(clue_size == first_rec_size);
	assert(clue_size == last_rec_size);
	assert(clue_size == (SIZEOF(gv_key) + keysize));
	gvt->clue.top = gvt->last_rec->top = gvt->first_rec->top = keysize;
	gvt->clue.prev = gvt->clue.end = 0;
	/* If "reg" is non-NULL, but "gvent" is NULL, then it means the targ_alloc is being done for the directory tree.
	 * In that case, set gvt->root appropriately to DIR_ROOT. Else set it to 0. Also assert that the region is
	 * open in this case with the only exception being if called from mur_forward for non-invasive operations (e.g. EXTRACT).
	 */
	assert((NULL != gvent) || (NULL == reg) || reg->open || (IS_MUPIP_IMAGE && !mupip_jnl_recover));
	gvt->root = ((NULL != reg) && (NULL == gvent) ? DIR_ROOT : 0);
	gvt->nct = 0;
	gvt->act = 0;
	gvt->ver = 0;
	gvt->regcnt = 1;
	gvt->collseq = NULL;
	gvt->read_local_tn = (trans_num)0;
	GTMTRIG_ONLY(gvt->trig_local_tn = (trans_num)0);
	gvt->noisolation = FALSE;
	gvt->alt_hist = (srch_hist *)malloc(SIZEOF(srch_hist));
	gvt->hist.h[0].blk_num = HIST_TERMINATOR;
	gvt->alt_hist->h[0].blk_num = HIST_TERMINATOR;
	/* Initialize the 0:MAX_BT_DEPTH. Otherwise, memove of the array in mu_reorg can cause problem */
	for (index = 0; index <= MAX_BT_DEPTH; index++)
	{
		gvt->hist.h[index].level = index;	/* needed in TP to know what level a history element is */
		gvt->hist.h[index].blk_target = gvt;
		gvt->alt_hist->h[index].level = index;
		gvt->alt_hist->h[index].blk_target = gvt;
	}
	gvt->split_cleanup_needed = FALSE;
	assert(ARRAYSIZE(gvt->last_split_direction) == ARRAYSIZE(gvt->last_split_blk_num));
	for (index = 0; index < ARRAYSIZE(gvt->last_split_direction); index++)
	{
		gvt->last_split_direction[index] = NEWREC_DIR_FORCED;
		gvt->last_split_blk_num[index] = 0;
	}
	gvt->prev_gvnh = NULL;
	gvt->next_tp_gvnh = NULL;
	assert(gv_target_list != gvt);
#	ifdef GTM_TRIGGER
	gvt->gvt_trigger = NULL;
	gvt->db_trigger_cycle = 0;
	gvt->db_dztrigger_cycle = 0;
	gvt->trig_mismatch_test_done = FALSE;
#	endif
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
	if (gvt == gv_target)
	{	/* Should not be freeing an actively used global variable. Exceptions are
		 *      a) We are exiting and in the process of freeing up cs_addrs->dir_tree in gv_rundown
		 *	b) The GT.CM GNP server which could free up a region as part of running down a client's database.
		 *	c) In VMS, DAL calls could rundown the database. This is tough to check using an assert.
		 * Assert accordingly.
		 */
		UNIX_ONLY(assert(IS_GTCM_GNP_SERVER_IMAGE
				 || (process_exiting && ((gvt == cs_addrs->dir_tree)
							 GTMTRIG_ONLY(|| (gvt == cs_addrs->hasht_tree))))));
		gv_target = NULL;	/* In that case, set gv_target to NULL to ensure freed up memory is never used */
	}
	/* assert we never delete a gvt that is actively used in a TP transaction */
	DBG_CHECK_IN_GVT_TP_LIST(gvt, FALSE);	/* FALSE => we check that gvt is NOT present in the gvt_tp_list */
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
