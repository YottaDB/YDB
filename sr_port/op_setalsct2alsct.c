/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
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
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"
#include "min_max.h"

GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;

/* Operation - Copy alias container to another alias container
 * Note that this cannot happen as the result of a normal copy via regular SET command (we do not allow it).
 */
void op_setalsct2alsct(lv_val *srclv, lv_val *dstlv)
{
	lv_val		*src_lvref, *src_lvbase, *dst_lvbase;
	symval		*sym_src_lvref, *sym_srclv, *sym_dstlv;

	error_def(ERR_ALIASEXPECTED);

	assert(srclv);
	assert(!LV_IS_BASE_VAR(srclv));	/* Verify subscripted var */
	assert(dstlv);
	assert(!LV_IS_BASE_VAR(dstlv));	/* Verify subscripted var */
	if (!(srclv->v.mvtype & MV_ALIASCONT))
		rts_error(VARLSTCNT(1) ERR_ALIASEXPECTED);
	src_lvref = (lv_val *)srclv->v.str.addr;
	assert(src_lvref);
	assert(LV_IS_BASE_VAR(src_lvref));	/* Verify base var */
	if (srclv != dstlv)
	{
		dst_lvbase = LV_GET_BASE_VAR(dstlv);
		if (dollar_tlevel && (NULL != dst_lvbase->tp_var) && !dst_lvbase->tp_var->var_cloned)
			TP_VAR_CLONE(dst_lvbase);	/* clone the tree. */
		dstlv->v = srclv->v;
		assert(0 < src_lvref->stats.trefcnt);
		assert(0 <= src_lvref->stats.crefcnt);
		INCR_TREFCNT(src_lvref);
		INCR_CREFCNT(src_lvref);
		/* Potentially 3 symvals need to be marked active. Pick the oldest
		 * of them to feed to the macro to get them all marked.
		 */
		src_lvbase = LV_GET_BASE_VAR(srclv);
		sym_srclv = LV_GET_SYMVAL(src_lvbase);
		sym_dstlv = LV_GET_SYMVAL(dst_lvbase);
		sym_src_lvref = LV_GET_SYMVAL(src_lvref);
		MARK_ALIAS_ACTIVE(MIN(MIN(sym_srclv->symvlvl, sym_dstlv->symvlvl), sym_src_lvref->symvlvl));
		/* Last operation is to mark the base var for our container array that it now has a container in it. */
		MARK_CONTAINER_ONBOARD(dst_lvbase);
	}
	assert(src_lvref->stats.trefcnt >= src_lvref->stats.crefcnt);
}
