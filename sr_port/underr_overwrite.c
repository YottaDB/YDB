/****************************************************************
 *								*
 * Copyright (c) 2012-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>

/* This is a slight variation of underr in which we copy
 * literal_null into the variable instead of returning a pointer
 * to literal_null itself. */
mval *underr_overwrite(mval *start, ...)
{
	va_list		var;

	va_start(var, start);
	*start = *underr(start);
	va_end(var);
	return start;
}
