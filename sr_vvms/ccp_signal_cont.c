/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

void ccp_signal_cont(uint4 arg1, ...)
{
	va_list	var;
	uint4	arg2, arg3, arg4, arg5;
	int4	numargs, pc;

	error_def(ERR_CCPSIGCONT);

	pc = caller_id();
	VAR_START(var, arg1);
	va_count(numargs);
	switch (numargs)
	{
	case 1:
		lib$signal(ERR_CCPSIGCONT, 1, pc, arg1);
		break;
	case 2:
		arg2 = va_arg(var, uint4);
		lib$signal(ERR_CCPSIGCONT, 1, pc, arg1, arg2);
		break;
	case 3:
		arg2 = va_arg(var, uint4);
		arg3 = va_arg(var, uint4);
		lib$signal(ERR_CCPSIGCONT, 1, pc, arg1, arg2, arg3);
		break;
	case 4:
		arg2 = va_arg(var, uint4);
		arg3 = va_arg(var, uint4);
		arg4 = va_arg(var, uint4);
		lib$signal(ERR_CCPSIGCONT, 1, pc, arg1, arg2, arg3, arg4);
		break;
	default:
		arg2 = va_arg(var, uint4);
		arg3 = va_arg(var, uint4);
		arg4 = va_arg(var, uint4);
		arg5 = va_arg(var, uint4);
		lib$signal(ERR_CCPSIGCONT, 1, pc, arg1, arg2, arg3, arg4, arg5);
		break;
	}
	va_end(var);
}
