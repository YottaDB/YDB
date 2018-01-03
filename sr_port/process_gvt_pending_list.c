/****************************************************************
 *								*
 * Copyright (c) 2008-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "dpgbldir.h"
#include "process_gvt_pending_list.h"
#include "targ_alloc.h"
#include "buddy_list.h"
#include "hashtab_mname.h"

GBLREF	gvt_container	*gvt_pending_list;
GBLREF	buddy_list	*gvt_pending_buddy_list;

gvt_container *is_gvt_in_pending_list(gv_namehead *gvt)
{
	gvt_container	*gvtc;

	for (gvtc = gvt_pending_list; NULL != gvtc; gvtc = (gvt_container *)gvtc->next_gvtc)
	{
		if (*gvtc->gvt_ptr == gvt)
			return gvtc;
	}
	return NULL;
}

/* Now that "reg" is being opened, process list of gv_targets that were allocated BEFORE reg got opened to see
 * if they need to be re-allocated (due to differences in reg->max_key_size versus csa->hdr->max_key_size).
 */
void process_gvt_pending_list(gd_region *reg, sgmnt_addrs *csa)
{
	gvt_container		*gvtc, *next_gvtc, *prev_gvtc;
	gv_namehead		*old_gvt, *new_gvt, *gvtarg;
	int4			db_max_key_size;
	boolean_t		added, first_wasopen;
	ht_ent_mname		*stayent, *old_gvt_ent;
	hash_table_mname	*gvt_hashtab;
	gd_addr			*gd_iter;
#	ifdef DEBUG
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	endif
	for (prev_gvtc = NULL, gvtc = gvt_pending_list; NULL != gvtc; gvtc = next_gvtc)
	{
		/* note down next_gvtc first thing before any continues or frees as next iteration relies on this */
		next_gvtc = (gvt_container *)gvtc->next_gvtc;
		old_gvt = *gvtc->gvt_ptr;
		assert(NULL == old_gvt->gd_csa);
		if (reg != gvtc->gd_reg)
		{
			prev_gvtc = gvtc;
			continue;
		}
		old_gvt->gd_csa = csa;	/* set csa now that we have just opened the region */
		db_max_key_size = csa->hdr->max_key_size;
		/* Remove the to-be-processed gvtc from the linked list since TARG_FREE_IF_NEEDED relies on this */
		if (NULL == prev_gvtc)
			gvt_pending_list = next_gvtc;	/* removing first element in linked list */
		else
		{
			assert(prev_gvtc->gd_reg != reg);
			prev_gvtc->next_gvtc = (struct gvt_container_struct *)next_gvtc;
		}
		if (NULL != (gvt_hashtab = csa->gvt_hashtab))
			added = add_hashtab_mname(gvt_hashtab, &old_gvt->gvname, old_gvt, &stayent);
		else
		{
			added = TRUE;	/* even though there is no hashtable set added so we go through the appropriate codepath */
			stayent = NULL;
		}
		if (added)
		{	/* Global name not already present in csa->gvt_hashtab. So old_gvt got added into the hashtable.
			 * But before that, check for differing key sizes between reg and db hdr in which case we need to
			 * allocate a new_gvt (with the bigger key size) and add new_gvt instead into csa->gvt_hashtab.
			 */
			if (reg->max_key_size != db_max_key_size)
			{	/* key sizes are different, need to reallocate */
				assert(IS_REG_BG_OR_MM(reg));
				new_gvt = (gv_namehead *)targ_alloc(db_max_key_size, &old_gvt->gvname, reg);
				new_gvt->noisolation = old_gvt->noisolation;	/* Copy over noisolation status from old_gvt */
				new_gvt->act = old_gvt->act; /* copy over act,nct,ver from old_gvt (actually from the gld file) */
				new_gvt->nct = old_gvt->nct;
				new_gvt->ver = old_gvt->ver;
				assert(!reg->open);
				/* reg is not yet open but we know csa is already available so set it appropriately */
				new_gvt->gd_csa = csa;
				if (NULL != stayent)
					stayent->value = new_gvt;
			} else
				new_gvt = NULL; /* No change to any gvt. Reset new_gvt to prevent allocation/free done later. */
		} else
		{	/* Global name already present in csa->gvt_hashtab. No need to allocate new_gvt.
			 * Only need to free up old_gvt. Also increment gvt->regcnt.
			 * If NOISOLATION status differs between the two, choose the more pessimistic one.
			 */
			new_gvt = (gv_namehead *)stayent->value;
			assert(new_gvt != old_gvt);
			if (FALSE == old_gvt->noisolation)
				new_gvt->noisolation = FALSE;
			assert(1 <= new_gvt->regcnt);
			new_gvt->regcnt++;
		}
		if (NULL != new_gvt)
		{
			*gvtc->gvt_ptr = new_gvt;	/* change hash-table to eventually point to new gvt */
			if (NULL != gvtc->gvt_ptr2)
			{	/* this is true only for spanning globals. Update one more location to point to new gvt */
				assert(TREF(spangbl_seen));
				assert(old_gvt == *gvtc->gvt_ptr2);
				*gvtc->gvt_ptr2 = new_gvt;
			}
			assert(1 == old_gvt->regcnt); /* assert that TARG_FREE will happen below */
			for (gd_iter = get_next_gdr(NULL); gd_iter; gd_iter = get_next_gdr(gd_iter))
			{	/* If gd_iter->tab_ptr still has old_gvt, remove it before freeing old_gvt. */
				old_gvt_ent = (ht_ent_mname *)lookup_hashtab_mname(gd_iter->tab_ptr, &old_gvt->gvname);
				if (old_gvt_ent)
					delete_hashtab_ent_mname(gd_iter->tab_ptr, old_gvt_ent);
			}
			TARG_FREE_IF_NEEDED(old_gvt);
		}
		/* else: new_gvt is NULL which means old_gvt stays as is */
		free_element(gvt_pending_buddy_list, (char *)gvtc);
	}
}
