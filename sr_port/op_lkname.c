/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <varargs.h>

#include "mlk_pvtblk_create.h"

void	op_lkname(va_alist)
va_dcl
{
	va_list	var;

	VAR_START(var);
	mlk_pvtblk_create(var);
}
