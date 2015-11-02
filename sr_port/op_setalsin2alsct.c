/****************************************************************
 *								*
 *	Copyright 2009, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "rtnhdr.h"
#include "stack_frame.h"
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

GBLREF stack_frame	*frame_pointer;
GBLREF symval		*curr_symval;
GBLREF short		dollar_tlevel;

LITREF mval 		literal_null;

/* Operation - The destination variable becomes a container variable holding a pointer to source alias

   1) Convert the src lvval to an alias variable if necessary (by manipulating the reference counts
   2) Save the address of the source lv_val in the destination var, setting the mval container flag.
   3) Whichever symval of source or destination is greatest, that is the earliest symval affected. We
      feed that to the MARK_ALIAS_ACTIVE macro to mark all interveening symvals as having alias
      activity and that covers both potential symvals so we don't have to call the macro twice.
*/
void op_setalsin2alsct(lv_val *srclv, lv_val *dstlv)
{
	lv_val		*tp_val;
 	lv_sbs_tbl	*tbl;
	symval		*sym;

	assert(srclv);
	assert(MV_SYM == srclv->ptrs.val_ent.parent.sym->ident);	/* Verify base var */
	assert(dstlv);
	assert(MV_SBS == dstlv->ptrs.val_ent.parent.sbs->ident);  	/* Verify subscripted var */
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
	/* Decrement alias container refs (if any) and cleanup if necessary */
	DECR_AC_REF(dstlv, TRUE);
	/* Reset value of lv_val to now be a container ref to the supplied base var */
	memcpy(&dstlv->v, &literal_null, SIZEOF(mval));
	dstlv->v.mvtype |= MV_ALIASCONT;				/* Set the magic container flag */
	dstlv->v.str.addr = (char *)srclv;				/* And save our reference */
	assert(0 < srclv->stats.trefcnt);
	assert(0 <= srclv->stats.crefcnt);
	INCR_TREFCNT(srclv);						/* Total reference counts */
	INCR_CREFCNT(srclv);						/* .. and a contain reference at that.. */
	assert(srclv->stats.trefcnt >= srclv->stats.crefcnt);
	/* These symvals have had alias activity */
	MARK_ALIAS_ACTIVE(MIN(srclv->ptrs.val_ent.parent.sym->symvlvl, dstlv->ptrs.val_ent.parent.sbs->sym->symvlvl));
	/* Last operation is to mark the base var for our container array that it now has a container in it.
	   But first it must be found by going backwards through the levels.
	*/
	MARK_CONTAINER_ONBOARD(dstlv);
}
