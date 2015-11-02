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

/* fgn_glopref : prefixes the global variable name with ^
	for locks.
*/
#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include <rtnhdr.h>
#include "lv_val.h"	/* needed by "fgncal.h" */
#include "fgncal.h"

GBLREF spdesc	stringpool ;

void fgn_glopref(mval *v)
{
	unsigned char *p;

	ENSURE_STP_FREE_SPACE(v->str.len + 1);
	p = stringpool.free;
	*stringpool.free++ = '^';
	memcpy(stringpool.free,v->str.addr,v->str.len);
	stringpool.free += v->str.len ;
	v->str.addr = (char *)p;
	v->str.len++;
}
