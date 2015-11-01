/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashtab.h"
#include "hashtab_mname.h"	/* needed for lv_val.h */
#include "lv_val.h"
#include "op.h"

GBLREF symval *curr_symval;

void op_killall(void)
{
	ht_ent_mname	*sym, *top;
	lv_val		*lv;
	for (sym = curr_symval->h_symtab.base, top = sym + curr_symval->h_symtab.size; sym < top; sym++)
	{
		if(HTENT_VALID_MNAME(sym, lv_val, lv))
			op_kill(lv);
	}
	return;
}
