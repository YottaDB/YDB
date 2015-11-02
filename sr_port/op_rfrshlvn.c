/****************************************************************
 *								*
 *	Copyright 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include <rtnhdr.h>
#include "valid_mname.h"
#include "gtm_string.h"
#include "cachectl.h"
#include "gtm_text_alloc.h"
#include "callg.h"
#include "mdq.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "min_max.h"
#include "glvn_pool.h"

GBLREF	bool			undef_inhibit;
GBLREF	symval			*curr_symval;

error_def(ERR_UNDEF);

/* [Used by FOR, SET and $ORDER()] Looks up a saved local variable. */
lv_val *op_rfrshlvn(uint4 indx, opctype oc)
{
	glvn_pool_entry		*slot;
	ht_ent_mname		*tabent;
	mname_entry		targ_key;
	lv_val			*lv;
	lvname_info		*lvn_info;
	unsigned char		buff[512], *end;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	slot = &(TREF(glvn_pool_ptr))->slot[indx];
	assert(OC_SAVLVN == slot->sav_opcode);
	targ_key.var_name = slot->lvname->str;
	COMPUTE_HASH_MNAME(&targ_key);
	targ_key.marked = FALSE;
	if (add_hashtab_mname_symval(&curr_symval->h_symtab, &targ_key, NULL, &tabent))
		lv_newname(tabent, curr_symval);
	lvn_info = (lvname_info *)&slot->glvn_info;
	lvn_info->start_lvp = (lv_val *)tabent->value;
	switch (oc)
	{
	case OC_RFRSHLVN: /* no UNDEF for $ORDER()/$GET() from srchindx nor bogus opcode, so borrow "impossible" opcode */
		return (lv_val *)callg((callgfnptr)op_srchindx, (gparam_list *)lvn_info);
		break;
	case OC_PUTINDX:
		lv = (lv_val *)callg((callgfnptr)op_putindx, (gparam_list *)lvn_info);
		break;
	case OC_SRCHINDX:
		lv = (lv_val *)callg((callgfnptr)op_srchindx, (gparam_list *)lvn_info);
		if (NULL == lv)
		{	/* This path is currently only used by FOR. Issue UNDEF error even if NOUNDEF is enabled. */
			end = format_key_mvals(buff, SIZEOF(buff), lvn_info);
			rts_error(VARLSTCNT(4) ERR_UNDEF, 2, end - buff, buff);
			assert(FALSE);
		}
		break;
	case OC_M_SRCHINDX:
		/* not currently used */
		lv = (lv_val *)callg((callgfnptr)op_m_srchindx, (gparam_list *)lvn_info);
		break;
	default:
		GTMASSERT;
	}
	assert(lv);
	return lv;
}
