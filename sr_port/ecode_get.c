/****************************************************************
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include "error_trap.h"

GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */

void ecode_get(int level, mval *val)
{
	mstr	tmpmstr;

	val->mvtype = MV_STR;
	assert(-1 == level);	/* currently -1 is the only valid negative level argument to ecode_get() */
	if (dollar_ecode.index)
	{
		assert(dollar_ecode.end > dollar_ecode.begin);
		val->str.addr = dollar_ecode.begin;
		val->str.len = INTCAST(dollar_ecode.end - dollar_ecode.begin + 1);	/* to account for terminating ',' */
		s2pool(&val->str);
	} else
	{
		assert(dollar_ecode.end == dollar_ecode.begin);
		val->str.len = 0;
	}
}
