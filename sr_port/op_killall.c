/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "op.h"

GBLREF symval *curr_symval;

/* Note similar code exists in op_xkill which excepts some records */
void op_killall(void)
{
	ht_ent_mname	*sym, *top;
	lv_val		*lv;

	for (sym = curr_symval->h_symtab.base, top = sym + curr_symval->h_symtab.size; sym < top; sym++)
	{
		if (HTENT_VALID_MNAME(sym, lv_val, lv))
			lv_kill(lv, DOTPSAVE_TRUE, DO_SUBTREE_TRUE);
	}
	return;
}
