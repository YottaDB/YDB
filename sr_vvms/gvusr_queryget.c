/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "str2gvkey.h"
#include "gvusr.h"
#include "gvusr_queryget.h"
#include "stringpool.h"

GBLREF gv_key	*gv_currkey;
GBLREF gv_key	*gv_altkey;

boolean_t gvusr_queryget(mval *v)
{ /* this function logically should be in DDPGVUSR, but is now in GTMSHR because the tree of functions called from
   * str2gvkey_nogvfunc use globals that are currently not setup in DDPGVUSR, and the number of globals makes for nasty problems
   * in the link */

	/* $Q and $G as separate steps, if $G returns "", loop until $Q returns "" or $Q and $G return non "" */

	for (; ;)
	{
		if (gvusr_query(v))
		{
			str2gvkey_nogvfunc(v->str.addr, v->str.len, gv_currkey); /* setup gv_currkey from return of query */
			if (gvusr_get(v))
			{
				s2pool(&v->str);
				return TRUE;
			}
		} else
			return FALSE;
	}
}
