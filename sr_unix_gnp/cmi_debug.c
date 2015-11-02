/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <stdarg.h>
#include <errno.h>
#include "gtmio.h"
#include "gtm_stdio.h"
#include "have_crit.h"

#include "cmidef.h"

#ifdef GTCM_CMI_DEBUG
GBLDEF int cmi_debug_enabled = TRUE;
#else
GBLDEF int cmi_debug_enabled = FALSE;
#endif

void cmi_dprint(char *cs, ...)
{
	va_list ap;
	int	rc;

#ifdef DEBUG
	VAR_START(ap, cs);
	VFPRINTF(stderr, cs, ap, rc);
	assert(0 <= rc);
	va_end(ap);
	FFLUSH(stderr);
#endif
}
