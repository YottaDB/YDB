/****************************************************************
 *								*
 *	Copyright 2010, 2013 Fidelity Information Services, Inc	*
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
#include "stack_frame.h"

GBLREF symval		*curr_symval;
GBLREF uint4		dollar_tlevel;
GBLREF stack_frame	*frame_pointer;
GBLREF mval		*alias_retarg;

/*  Operation - Set alias container returned from a function call into another alias container
 *
 *  Note that this opcode's function is very similar to op_setalsct2alsct() but is necessarily different because the
 *  source container is a temporary mval passed back through a function return rather than the lv_val op_setalsct2alsct()
 *  deals with. Consequently, the amount of verification we can do is reduced. But this is acceptable due to the checks
 *  done by unw_retarg() and op_exfunretals() which pre-processed this value for us. There is also different reference
 *  count maintenance to do than the op_setalsct2alsct() opcode. With substantially more work to reorganize how SET
 *  operates, it would likely be possible to combine these functions but the way things are structured now, all the
 *  set functions plus op_sto() share the same API so adding a parm to one means adding a useless parm to all 6 of
 *  them which is not acceptable so we end up duplicating portions of code here.
 */
void op_setfnretin2alsct(mval *srcmv, lv_val *dstlv)
{
	lv_val		*src_lvref, *dst_lvbase;
	symval		*sym_src_lvref, *sym_dstlv;

	assert(srcmv);
	assert(dstlv);
	assert(srcmv == alias_retarg);
	assert(!LV_IS_BASE_VAR(dstlv));	/* Verify subscripted var */
	assert(srcmv->mvtype & MV_ALIASCONT);
	assert(MVAL_IN_RANGE(srcmv, frame_pointer->temps_ptr, frame_pointer->temp_mvals));	/* Verify is a temp mval */
	assert(MV_IS_STRING(srcmv) && (0 == srcmv->str.len));
	src_lvref = (lv_val *)srcmv->str.addr;
	assert(src_lvref);
	assert(LV_IS_BASE_VAR(src_lvref));	/* Verify base var */
	assert(srcmv != (mval *)dstlv);	/* Equality not possible since src is a temp mval and destination cannot be */
	dst_lvbase = LV_GET_BASE_VAR(dstlv);
	if (dollar_tlevel && (NULL != dst_lvbase->tp_var) && !dst_lvbase->tp_var->var_cloned)
		TP_VAR_CLONE(dst_lvbase);	/* clone the tree. */
	/* Note reference counts were increased in unw_retarg() to preserve the values the input container pointed to
	 * when they potentially went out of scope during the return. So we do not increment them further here.
	 */
	DBGRFCT((stderr, "op_setfnretin2alsct: Copying funcret container referencing lvval 0x"lvaddr" into container 0x"lvaddr
		 "\n", src_lvref, dstlv));
	dstlv->v = *srcmv;
	assert(0 < src_lvref->stats.trefcnt);
	assert(0 <= src_lvref->stats.crefcnt);
	assert(src_lvref->stats.trefcnt >= src_lvref->stats.crefcnt);
	/* Mark as alias-active the symval(s) of the destination container's and the source's pointed-to arrays */
	sym_dstlv = LV_GET_SYMVAL(dst_lvbase);
	sym_src_lvref = LV_GET_SYMVAL(src_lvref);
	MARK_ALIAS_ACTIVE(MIN(sym_dstlv->symvlvl, sym_src_lvref->symvlvl));
	/* Last operation is to mark the base var for our container array that it now has a container in it. */
	MARK_CONTAINER_ONBOARD(dst_lvbase);
	alias_retarg = NULL;
}
