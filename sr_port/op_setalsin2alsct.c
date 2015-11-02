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
#include "gtm_string.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
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
GBLREF uint4		dollar_tlevel;

LITREF mval 		literal_null;

/* Operation - The destination variable becomes a container variable holding a pointer to source alias
 *
 * 1) Convert the src lvval to an alias variable if necessary (by manipulating the reference counts
 * 2) Save the address of the source lv_val in the destination var, setting the mval container flag.
 * 3) Whichever symval of source or destination is greatest, that is the earliest symval affected. We
 *    feed that to the MARK_ALIAS_ACTIVE macro to mark all intervening symvals as having alias
 *    activity and that covers both potential symvals so we don't have to call the macro twice.
 */
void op_setalsin2alsct(lv_val *srclv, lv_val *dstlv)
{
	lv_val		*dst_lvbase;
	symval		*sym_srclv, *sym_dstlv;

	assert(srclv);
	assert(LV_IS_BASE_VAR(srclv));	/* Verify base var */
	assert(dstlv);
	assert(!LV_IS_BASE_VAR(dstlv));	/* Verify subscripted var */
	dst_lvbase = LV_GET_BASE_VAR(dstlv);
	if (dollar_tlevel && (NULL != dst_lvbase->tp_var) && !dst_lvbase->tp_var->var_cloned)
		TP_VAR_CLONE(dst_lvbase);	/* clone the tree. */
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
	sym_srclv = LV_GET_SYMVAL(srclv);
	sym_dstlv = LV_GET_SYMVAL(dst_lvbase);
	MARK_ALIAS_ACTIVE(MIN(sym_srclv->symvlvl, sym_dstlv->symvlvl));
	/* Last operation is to mark the base var for our container array that it now has a container in it.
	 * But first it must be found by going backwards through the levels.
	 */
	MARK_CONTAINER_ONBOARD(dst_lvbase);
}
