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
#ifdef DEBUG
#include "mdef.h"
#ifdef EARLY_VARARGS
#include <varargs.h>
#endif
#include "gtm_stdio.h"
#include "cmidef.h"
#ifndef EARLY_VARARGS
#include <varargs.h>
#endif

GBLDEF int cmi_debug_enabled = FALSE;

void cmi_dprint(va_alist)
va_dcl
{
	va_list ap;
	char *cs;

	VAR_START(ap);
	cs = va_arg(ap, char *);
	vfprintf(stderr, cs, ap);
	fflush(stderr);
}
#endif
