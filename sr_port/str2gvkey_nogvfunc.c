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

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "subscript.h"
#include "str2gvargs.h"
#include "str2gvkey.h"

void str2gvkey_nogvfunc(char *cp, int len, gv_key *key)
{ /* We have two functions str2gvkey_gvfunc and str2gvkey_nogvfunc instead of passing a boolean argument to say a common  */
  /* function str2gvkey because GVUSR_QUERYGET when moved to DDPGVUSR would pull in op_gvname and op_gvargs objects - will */
  /* cause nasty problems in the link */

	boolean_t	naked;
	gvargs_t	op_gvargs;
	int		subsc;

	naked = str2gvargs(cp, len, &op_gvargs);
	if (naked)
		GTMASSERT; /* 'cos this function does not handle nakeds correctly */
	key->end = op_gvargs.args[0]->str.len;
	memcpy(key->base, op_gvargs.args[0]->str.addr, key->end);
	key->base[key->end] = 0;
	key->end++;
	key->prev = 0;
	for (subsc = 1; subsc < op_gvargs.count; subsc++)
		mval2subsc(op_gvargs.args[subsc], key);
	return;
}
