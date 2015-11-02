/****************************************************************
 *								*
 *	Copyright 2008, 2010 Fidelity Information Services, Inc.	*
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

GBLREF	gvt_container	*gvt_pending_list;
GBLREF	buddy_list	*gvt_pending_buddy_list;

boolean_t	is_gvt_in_pending_list(gv_namehead *gvt)
{
	gvt_container	*gvtc;

	for (gvtc = gvt_pending_list; NULL != gvtc; gvtc = (gvt_container *)gvtc->next_gvtc)
	{
		if (gvtc->gvnh_reg->gvt == gvt)
			return TRUE;
	}
	return FALSE;
}

/* Now that "reg" is being opened, process list of gv_targets that were allocated BEFORE reg got opened to see if they
 * need to be re-allocated (due to differences in reg->max_key_size versus csa->hdr->max_key_size.
 */
void process_gvt_pending_list(gd_region *reg, sgmnt_addrs *csa)
{
	gvt_container	*gvtc, *next_gvtc, *prev_gvtc;
	gv_namehead	*old_gvt, *new_gvt;
	int4		db_max_key_size;
	gvnh_reg_t	*gvnh_reg;

	for (prev_gvtc = NULL, gvtc = gvt_pending_list; NULL != gvtc; gvtc = next_gvtc)
	{
		/* note down next_gvtc first thing before any continues or frees as next iteration relies on this */
		next_gvtc = (gvt_container *)gvtc->next_gvtc;
		gvnh_reg = gvtc->gvnh_reg;
		old_gvt = gvnh_reg->gvt;
		assert(NULL == old_gvt->gd_csa);
		if (reg != gvnh_reg->gd_reg)
		{
			prev_gvtc = gvtc;
			continue;
		}
		old_gvt->gd_csa = csa;	/* set csa now that we are about to open the region */
		db_max_key_size = csa->hdr->max_key_size;
		if (reg->max_key_size != db_max_key_size)
		{	/* key sizes are different, so need to reallocate */
			new_gvt = (gv_namehead *)targ_alloc(db_max_key_size, &old_gvt->gvname, reg);
			new_gvt->noisolation = old_gvt->noisolation;	/* Copy over noisolation status from old_gvt */
			assert(!reg->open);
			/* reg is not yet open but we know csa is already available so set it appropriately */
			new_gvt->gd_csa = csa;
			gvnh_reg->gvt = new_gvt;		/* Change hash-table to point to new gvt */
			old_gvt->regcnt--;
			targ_free(old_gvt);	/* Reposition pointers & freeup memory associated with old_gvt */
		}
		/* Free up the processed gvtc */
		if (NULL == prev_gvtc)
			gvt_pending_list = next_gvtc;	/* freeing up first element in linked list */
		else
		{
			assert(prev_gvtc->gvnh_reg->gd_reg != reg);
			prev_gvtc->next_gvtc = (struct gvt_container_struct *)next_gvtc;
		}
		free_element(gvt_pending_buddy_list, (char *)gvtc);
	}
}
