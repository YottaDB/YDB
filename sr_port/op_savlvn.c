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

GBLREF	spdesc			stringpool;

/* [Used by FOR, SET and $ORDER()] Saves a local in the glvn pool and returns its index. */
void op_savlvn(UNIX_ONLY_COMMA(int argcnt) lv_val *start, ...)
{
	glvn_pool_entry		*slot;
	int			i;
	VMS_ONLY(int		argcnt;)
	lvname_info		*lvn_info;
	mident			*lvent;
	mname_entry		*targ_key;
	mval			*m, *key;
	unsigned char		*c, *ptr;
	va_list			var;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, start);
	VMS_ONLY(va_count(argcnt));
	ENSURE_GLVN_POOL_SPACE(argcnt);
	/* Get variable name and store it in the stringpool. */
	GET_GLVN_POOL_STATE(slot, m);
	assert(OC_SAVLVN == slot->sav_opcode);
	ENSURE_STP_FREE_SPACE(SIZEOF(mident_fixed));
	slot->lvname = m;
	m->mvtype = MV_STR; /* needs to be protected if garbage collection happens during s2pool below */
	lvent = &slot->lvname->str;
	ptr = stringpool.free;
	c = format_lvname(start, ptr, SIZEOF(mident_fixed));
	lvent->addr = (char *)ptr;
	lvent->len = (char *)c - (char *)ptr;
	stringpool.free = c;
	m++;
	(TREF(glvn_pool_ptr))->mval_top++;
	lvn_info = (lvname_info *)&slot->glvn_info;
	lvn_info->total_lv_subs = argcnt--;
	for (i = 0; i < argcnt; i++, m++)
	{	/* now all the pieces of the key */
		key = va_arg(var, mval *);
		*m = *key;
		lvn_info->lv_subs[i] = m;
		(TREF(glvn_pool_ptr))->mval_top++;
		if (MV_IS_STRING(m) && !IS_IN_STRINGPOOL(m->str.addr, m->str.len))
			s2pool(&m->str);
	}
}
