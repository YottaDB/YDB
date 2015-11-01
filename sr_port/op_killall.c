/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "hashdef.h"
#include "lv_val.h"
#include "op.h"

GBLREF symval *curr_symval;

void
op_killall(void)
{
	ht_entry *sym, *top;
	for (sym = curr_symval->h_symtab.base , top = sym + curr_symval->h_symtab.size ; sym  < top ; sym++)
		if (sym->nb.txt[0])
		{	op_kill((lv_val *)sym->ptr);
		}
	return;
}
