/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include "op.h"
#include "hashtab_mname.h"
#include "hashtab.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "min_max.h"

GBLREF symval		*curr_symval;
GBLREF short		dollar_tlevel;

/* Operation - Copy alias container to another alias container

   Note that this cannot happen as the result of a normal copy via regular SET command (we
   do not allow it).
*/
void op_setalsct2alsct(lv_val *srclv, lv_val *dstlv)
{
	lv_val		*lvref, *tp_val;
 	lv_sbs_tbl	*tbl;
	symval		*sym;

	error_def(ERR_ALIASEXPECTED);

	assert(srclv);
	assert(MV_SBS == srclv->ptrs.val_ent.parent.sbs->ident);  	/* Verify subscripted var */
	assert(dstlv);
	assert(MV_SBS == dstlv->ptrs.val_ent.parent.sbs->ident);  	/* Verify subscripted var */
	if (!(srclv->v.mvtype & MV_ALIASCONT))
		rts_error(VARLSTCNT(1) ERR_ALIASEXPECTED);
	lvref = (lv_val *)srclv->v.str.addr;
	assert(lvref);
	assert(MV_SYM == lvref->ptrs.val_ent.parent.sym->ident);	/* Verify base var */
	if (srclv != dstlv)
	{
		if (dollar_tlevel)
		{	/* Determine base var that needs to be cloned for this subscripted container var being modified */
			tp_val = dstlv;
		       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			while (MV_SYM != tbl->ident)
			{
				tp_val = tbl->lv;
			       	tbl = tp_val->ptrs.val_ent.parent.sbs;
			}
			if (NULL != tp_val->tp_var && !tp_val->tp_var->var_cloned)
				TP_VAR_CLONE(tp_val);	/* clone the tree. */
		}
		dstlv->v = srclv->v;
		assert(0 < lvref->stats.trefcnt);
		assert(0 <= lvref->stats.crefcnt);
		INCR_TREFCNT(lvref);
		INCR_CREFCNT(lvref);
		/* Potentially 3 symvals need to be marked active. Pick the oldest of them to feed to the
		   macro to get them all marked. */
		MARK_ALIAS_ACTIVE(MIN(MIN(srclv->ptrs.val_ent.parent.sbs->sym->symvlvl,
					  dstlv->ptrs.val_ent.parent.sbs->sym->symvlvl),
				      lvref->ptrs.val_ent.parent.sym->symvlvl));
	}
	assert(lvref->stats.trefcnt >= lvref->stats.crefcnt);
	/* Last operation is to mark the base var for our container array that it now has a container in it.
	   But first it must be found by going backwards through the levels.
	*/
	MARK_CONTAINER_ONBOARD(dstlv);
}
