/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
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
#include "stack_frame.h"

GBLREF symval		*curr_symval;
GBLREF short		dollar_tlevel;
GBLREF stack_frame	*frame_pointer;
GBLREF mval		*alias_retarg;

/*  Operation - Set alias container returned from a function call into another alias container
 *
 *  Note that this opcode's function is very similar to op_setalsct2alsct() but is necessarily different because the
 *  source container is a temporary mval passed back through a function return rather than the lv_val op_setalsct2alsct()
 *  deals with. Consequently, the amount of verification we can do reduced. But this is acceptable due to the checks
 *  done by unw_retarg() and op_exfunretals() which pre-processed this value for us. There is also different reference
 *  count maintenance to do than the op_setalsct2alsct() opcode. With substantially more work to reorganize how SET
 *  operates, it would likely be possible to combine these functions but the way things are structured now, all the
 *  set functions plus op_sto() share the same API so adding a parm to one means adding a useless parm to all 6 of
 *  them which is not acceptable so we end up duplicating portions of code here.
 */
void op_setfnretin2alsct(mval *srcmv, lv_val *dstlv)
{
	lv_val		*lvref, *tp_val;
 	lv_sbs_tbl	*tbl;
	symval		*sym;

	error_def(ERR_ALIASEXPECTED);

	assert(srcmv);
	assert(dstlv);
	assert(srcmv == alias_retarg);
	assert(MV_SBS == dstlv->ptrs.val_ent.parent.sbs->ident);  	/* Verify subscripted var */
	assert(srcmv->mvtype & MV_ALIASCONT);
	/* Verify is a temp mval */
	assert((char *)srcmv >= (char *)frame_pointer->temps_ptr
	       && (char *)srcmv < ((char *)frame_pointer->temps_ptr + (SIZEOF(char *) * frame_pointer->temp_mvals)));
	lvref = (lv_val *)srcmv->str.addr;
	assert(lvref);
	assert(MV_SYM == lvref->ptrs.val_ent.parent.sym->ident);	/* Verify base var */
	assert(srcmv != (mval *)dstlv);	/* Equality not possible since src is a temp mval and destination cannot be */
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
	/* Note reference counts were increased in unw_retarg() to preserve the values the input container pointed to
	 * when they potentially went out of scope during the return. So we do not increment them further here.
	 */
	DBGRFCT((stderr, "op_setfnretin2alsct: Copying funcret container referencing lvval 0x"lvaddr" into container 0x"lvaddr
		 "\n", lvref, dst_lv));
	dstlv->v = *srcmv;
	assert(0 < lvref->stats.trefcnt);
	assert(0 <= lvref->stats.crefcnt);
	assert(lvref->stats.trefcnt >= lvref->stats.crefcnt);
	/* Mark as alias-active the symval(s) of the destination container's and the source's pointed-to arrays */
	MARK_ALIAS_ACTIVE(MIN(dstlv->ptrs.val_ent.parent.sbs->sym->symvlvl, lvref->ptrs.val_ent.parent.sym->symvlvl));
	/* Last operation is to mark the base var for our container array that it now has a container in it.
	   But first it must be found by going backwards through the levels.
	*/
	MARK_CONTAINER_ONBOARD(dstlv);
	alias_retarg = NULL;
}
