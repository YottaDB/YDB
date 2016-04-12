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

error_def(ERR_INVSTRLEN);
error_def(ERR_BADTRNPARAM);

#define FULL 3
#define LENGTH 4
#define NO_ALIAS 6
#define TABLE_NAME 8
#define TERMINAL 9
#define VALUE 10

static readonly nametabent trnitm_table[] =
{
	{ 11, "ACCESS_MODE"},	{ 9, "CONCEALED"},
	{ 7, "CONFINE"},	{ 3, "FUL*"},
	{ 3, "LEN*"},		{ 9, "MAX_INDEX"},
	{ 8, "NO_ALIAS"},	{ 5, "TABLE"},
	{ 10, "TABLE_NAME"},	{ 3, "TER*"},
	{ 3, "VAL*"}
};

static readonly unsigned char trnitm_index[] =
{
	0, 1, 1, 3, 3, 3, 4,  4,  4,  4,  4,  4,  5,
	6, 7, 7, 7, 7, 7, 7, 10, 10, 11, 11, 11, 11
};

void op_fnztrnlnm(mval *name, mval *table, int4 ind, mval *mode, mval *case_blind, mval *item, mval *ret)
{
	char		buf[MAX_TRANS_NAME_LEN];
	char		*status;
	int		item_code;
	short		retlen;

	MV_FORCE_STR(name);
	if (name->str.len >= MAX_TRANS_NAME_LEN)
		rts_error(VARLSTCNT(4) ERR_INVSTRLEN, 2, name->str.len, MAX_TRANS_NAME_LEN - 1);
	MV_FORCE_STR(item);
	if (item->str.len)
	{
		if ((item_code = namelook(trnitm_index, trnitm_table, item->str.addr, item->str.len)) < 0) /* NOTE assignment */
			rts_error(VARLSTCNT(4) ERR_BADTRNPARAM,2,item->str.len,item->str.addr);
	} else
		item_code = VALUE;
	ret->mvtype = MV_STR;
	memcpy(buf, name->str.addr, name->str.len);
	buf[name->str.len] = 0;
	status = GETENV(buf);
	switch (item_code)
	{
		case FULL:
		case VALUE:
			if (status)
			{
				retlen = strlen(status);
				ENSURE_STP_FREE_SPACE(retlen);
				ret->str.addr = (char *)stringpool.free;
				ret->str.len = retlen;
				memcpy(ret->str.addr, status, retlen);
				stringpool.free += retlen;
			} else
				ret->str.len = 0;
			break;
		case LENGTH:
			if (status)
			{
				MV_FORCE_MVAL(ret, STRLEN(status));
				n2s(ret);
			} else
				ret->str.len = 0;
			break;
		case NO_ALIAS:
		case TERMINAL:
			MV_FORCE_MVAL(ret, ((NO_ALIAS == item_code) || status) ? 1 : 0);
			n2s(ret);
			break;
		default:
			if ((status) && (TABLE_NAME != item_code))
			{
				MV_FORCE_MVAL(ret, 0);
				n2s(ret);
			} else
				ret->str.len = 0;
	}
	return;
}
