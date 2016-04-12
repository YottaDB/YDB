/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

#include "mlk_pvtblk_create.h"
#include "op.h"

void	op_lkname(UNIX_ONLY_COMMA(int subcnt) mval *extgbl1, ...)
{
	VMS_ONLY(int subcnt;)
	va_list	var;

	VAR_START(var, extgbl1);
	VMS_ONLY(va_count(subcnt);)
	assert(2 <= subcnt);
	mlk_pvtblk_create(subcnt, extgbl1, var);
	va_end(var);
}
