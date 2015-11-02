/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*** STUB FILE ***/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "stringpool.h"
#include "nametabtyp.h"
#include "op.h"
#include "namelook.h"
#include "mvalconv.h"

GBLREF spdesc stringpool;

#define FULL 0
#define LENGTH 1
#define TERMINAL 2
#define VALUE 3
#define MAX_TRANS_LOG 256

static readonly nametabent trnitm_table[] =
{
	{ 3, "FUL*" },
	{ 3, "LEN*"},
	{ 3, "TER*"},
	{ 3, "VAL*"}
};
static readonly unsigned char trnitm_index[27] =
{
	0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2
	,2, 2, 2, 2, 2, 2, 3, 3, 4, 4, 4
	,4, 4, 4
};

void op_fnztrnlnm(mval *name, mval *table, int4 ind, mval *mode, mval *case_blind, mval *item, mval *ret)
{
	char		buf[MAX_TRANS_LOG+1];
	char		*status;
	int		item_code;
	short		retlen;

	error_def(ERR_INVSTRLEN);
	error_def(ERR_BADTRNPARAM);

	MV_FORCE_STR(name);
	MV_FORCE_STR(item);

	if (name->str.len > MAX_TRANS_LOG)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, name->str.len, MAX_TRANS_LOG);

	if (item->str.len)
	{
		if ((item_code = namelook(trnitm_index, trnitm_table, item->str.addr, item->str.len)) < 0)
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM,2,item->str.len,item->str.addr);
	}
	else
		item_code = VALUE;

	memcpy(buf, name->str.addr, name->str.len);
	buf [name->str.len] = 0;
	status = GETENV(buf);
	if (status)
	{
		if (item_code == VALUE || item_code == FULL)
		{
			retlen = strlen(status);
			ENSURE_STP_FREE_SPACE(retlen);
			ret->mvtype = MV_STR;
			ret->str.addr = (char *)stringpool.free;
			ret->str.len = retlen;
			memcpy(ret->str.addr, status, retlen);
			stringpool.free += retlen;
		}else if (item_code == LENGTH)
		{
			MV_FORCE_MVAL(ret, STRLEN(status));
			n2s(ret);
		}else
		{	assert(item_code == TERMINAL);
			MV_FORCE_MVAL(ret, 1);
			n2s(ret);
		}
	}else
	{	ret->mvtype = MV_STR;
		ret->str.len = 0;
	}
	return;
}
