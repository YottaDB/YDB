/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <rms.h>		/* needed for muextr.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"	/* above includes needed for muextr.h */
#include "muextr.h"	/* needed for "mu_extr_ident" prototype */

#define IDENT_FCHAR(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z') || (x) == '%')
#define IDENT_TAIL(x) (((x) >= 'A' && (x) <= 'Z') || ((x) >= 'a' && (x) <= 'z') || ((x) >= '0' && (x) <= '9'))

char *mu_extr_ident(mstr *a)
{
	char *c, *b;

	b = a->addr;
	c = b + a->len;
	if (IDENT_FCHAR(*b))
		while (++b < c)
			if (!IDENT_TAIL(*b))
				break;
	return b;
}
