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
#include "compiler.h"
#include "resolve_lab.h"

error_def(ERR_LABELMISSING);

void resolve_lab(mlabel *label, int *errknt)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (!label->ml)
	{
		(*errknt)++;
		stx_error(ERR_LABELMISSING, 2, label->mvname.len, label->mvname.addr);
		TREF(source_error_found) = 0;
	}
	return;
}
