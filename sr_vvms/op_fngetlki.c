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

#include "mdef.h"
#include <lkidef.h>
#include "op_fn.h"
#include "stringpool.h"
#include <ssdef.h>
#include <descrip.h>
#include "gtm_caseconv.h"
#include "mvalconv.h"
#include "mval2desc.h"
#include "op.h"

#define MAX_KEY_LEN 12
#define MAX_LKI_STRLEN 64
#define MAX_LKI_VALBLK 16
#define MIN_INDEX 0
#define MAX_INDEX 25

typedef struct
{	char	len;
	char	name[MAX_KEY_LEN];
	short int item_code;
} lki_tab;

typedef struct
{	char	index;
	char	len;
} lki_ind;

static readonly lki_tab lki_param_table[] =
{
	{  4, "CSID",          LKI$_CSID },
	{  8, "CVTCOUNT",      LKI$_CVTCOUNT },
	{ 10, "GRANTCOUNT",    LKI$_GRANTCOUNT },
	{  9, "LCKREFCNT",     LKI$_LCKREFCNT },
	{  4, "LKID",          LKI$_LKID },
	{  6, "LOCKID",        LKI$_LOCKID },
	{  7, "MSTCSID",       LKI$_MSTCSID },
	{  7, "MSTLKID",       LKI$_MSTLKID },
	{  8, "NAMSPACE",      LKI$_NAMSPACE },
	{  6, "PARENT",        LKI$_PARENT },
	{  3, "PID",           LKI$_PID },
	{  6, "RESNAM",        LKI$_RESNAM },
	{  9, "RSBREFCNT",     LKI$_RSBREFCNT },
	{  5, "STATE",         LKI$_STATE },
	{  6, "VALBLK",        LKI$_VALBLK },
	{  9, "WAITCOUNT",     LKI$_WAITCOUNT }
};

static readonly lki_ind lki_index_table[] =
{
	{ 0 , 0 }, { 0 , 0 }, { 0 , 2 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 2 , 3 },
	{ 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }, { 3 , 6 }, { 6 , 8 }, { 8 , 9 },
	{ 0 , 0 }, { 9 , 11 }, { 0 , 0 }, { 11 , 13 }, { 13 , 14 }, { 0 , 0 }, { 0 , 0 },
	{ 14 , 15 }, { 15 , 16 }, { 0 , 0 }, { 0 , 0 }, { 0 , 0 }
};

GBLREF spdesc stringpool;

error_def(ERR_BADLKIPARAM);


void op_fngetlki(mval *lkid_mval, mval *keyword, mval *ret)
{
	itmlist_struct	item_list;
	int		i;
	uint4		lkid, out_long, out_len, status, value_block[4];
	short int	index, slot, last_slot, lki_code;
	char		*p, upcase[MAX_KEY_LEN];
	struct dsc$descriptor lkid_desc;

assert (stringpool.free >= stringpool.base);
assert (stringpool.top >= stringpool.free);
ENSURE_STP_FREE_SPACE(MAX_LKI_STRLEN);
MV_FORCE_STR(keyword);
if (keyword->str.len == 0)
	rts_error(VARLSTCNT(4) ERR_BADLKIPARAM,2,4,"Null");
lower_to_upper(upcase,keyword->str.addr,keyword->str.len);
if ((index = upcase[0] - 'A') < MIN_INDEX || index > MAX_INDEX)
	rts_error(VARLSTCNT(4)  ERR_BADLKIPARAM,2,keyword->str.len,keyword->str.addr );
slot = lki_index_table[ index ].index;
last_slot = lki_index_table[ index ].len;
lki_code = 0;
for ( ; slot < last_slot ; slot++ )
{
	if (lki_param_table[ slot ].len == keyword->str.len
		&& !(memcmp(lki_param_table[ slot ].name,upcase,keyword->str.len)))
	{	lki_code = lki_param_table[ slot ].item_code;
		break;
	}
}
if (!lki_code)
	rts_error(VARLSTCNT(4)  ERR_BADLKIPARAM, 2, keyword->str.len, keyword->str.addr);
lkid_desc.dsc$b_class = DSC$K_CLASS_S;
lkid_desc.dsc$b_dtype = DSC$K_DTYPE_LU;
lkid_desc.dsc$w_length = SIZEOF(lkid);
lkid_desc.dsc$a_pointer = &lkid;
mval2desc(lkid_mval, &lkid_desc);
switch( lki_code )
{
case LKI$_VALBLK:
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.top >= stringpool.free);
	ENSURE_STP_FREE_SPACE(MAX_LKI_VALBLK*2);
	item_list.itmcode = lki_code;
	item_list.bufflen = MAX_LKI_VALBLK;
	item_list.buffaddr = &value_block[0];
	item_list.retlen = &out_len;
	item_list.end = NULL;

	if ((status = sys$getlkiw( 0, &lkid, &item_list, 0, 0, 0, 0)) != SS$_NORMAL)
	{	rts_error(VARLSTCNT(1)  status );
	}

	for (p = stringpool.free, i = 0; i < 4; i++, p += 8)
	  i2hex (value_block[i], p, 8);

	ret->mvtype = MV_STR;
	ret->str.addr = stringpool.free;
	ret->str.len = MAX_LKI_VALBLK*2;
	stringpool.free += MAX_LKI_VALBLK*2;
	assert (stringpool.free >= stringpool.base);
	assert (stringpool.top >= stringpool.free);
	return;

case LKI$_RESNAM:
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.top >= stringpool.free);
	ENSURE_STP_FREE_SPACE(MAX_LKI_STRLEN);
	item_list.itmcode = lki_code;
	item_list.bufflen = MAX_LKI_STRLEN;
	item_list.buffaddr = stringpool.free;
	item_list.retlen = &out_len;
	item_list.end = NULL;

	if ((status = sys$getlkiw( 0, &lkid, &item_list, 0, 0, 0, 0)) != SS$_NORMAL)
	{	rts_error(VARLSTCNT(1)  status );
	}

	ret->mvtype = MV_STR;
	ret->str.addr = stringpool.free;
	ret->str.len = out_len;		/* Note: this truncates the irrelevant high order word of out_len */
	stringpool.free += ret->str.len;
	assert (stringpool.free >= stringpool.base);
	assert (stringpool.top >= stringpool.free);
	return;

default:
	item_list.itmcode = lki_code;
	item_list.bufflen = 4;
	item_list.buffaddr = &out_long;
	item_list.retlen = &out_len;
	item_list.end = 0;

	if ((status = sys$getlkiw( 0, &lkid, &item_list, 0, 0, 0, 0)) != SS$_NORMAL)
	{	rts_error(VARLSTCNT(1)  status );
	}
	i2mval(ret,out_long) ;
	return;
}
}
