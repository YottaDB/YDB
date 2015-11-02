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
#include "compiler.h"
#include "resolve_lab.h"

GBLREF int4 source_error_found;

void resolve_lab(mlabel *label, int *errknt)
{
	error_def(ERR_LABELMISSING);

	if (!label->ml)
	{
		(*errknt)++;
		stx_error(ERR_LABELMISSING, 2, label->mvname.len, label->mvname.addr);
		source_error_found = 0;
	}
	return;
}
