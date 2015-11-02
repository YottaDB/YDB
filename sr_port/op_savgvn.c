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

/* [Used by SET] Saves a global in the glvn pool and returns its index. */
void op_savgvn(UNIX_ONLY_COMMA(int argcnt) mval *val_arg, ...)
{
	va_list			var;
	mval			*m, *key;
	glvn_pool_entry		*slot;
	gparam_list		*gvn_info;
	int			i;
	VMS_ONLY(int		argcnt;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	VAR_START(var, val_arg);
	VMS_ONLY(va_count(argcnt));
	ENSURE_GLVN_POOL_SPACE(argcnt);
	GET_GLVN_POOL_STATE(slot, m);
	gvn_info = (gparam_list *)&slot->glvn_info;
	gvn_info->n = argcnt;
	key = val_arg;
	for (i = 0; ; )
	{
		*m = *key;
		gvn_info->arg[i] = m;
		(TREF(glvn_pool_ptr))->mval_top++;
		if (MV_IS_STRING(m) && !IS_IN_STRINGPOOL(m->str.addr, m->str.len))
			s2pool(&m->str);
		m++;
		if (++i < argcnt)
			key = va_arg(var, mval *);
		else
			break;
	}
}
