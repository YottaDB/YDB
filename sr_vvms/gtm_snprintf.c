/****************************************************************
 *								*
 *	Copyright 2002, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"

#include <stdarg.h>

int gtm_snprintf(char *str, size_t size, const char *format, ...)
{ /* hack for VMS, ignore size argument and call sprintf. When snprintf becomes available on VMS, nix this file and define SNPRINTF
   * in gtm_stdio.h to snprintf */

	va_list	printargs;
	int	retval, rc;

	va_start(printargs, format);
	retval = VSPRINTF(str, format, printargs, rc);
	va_end(printargs);
	return retval;
}
