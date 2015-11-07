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

#include "mdef.h"
#include "op_fn.h"
#include <lkidef.h>
#include <ssdef.h>
#include "op.h"
#include "mvalconv.h"

static	bool	lock_used = FALSE;

void op_fnzlkid (mint boolex, mval *retval)
{
error_def(ERR_ZLKIDBADARG);
static	itmlist_struct	item_list;
static	int4	lock_id = -1;
static	int4	out_value;
static	int4	out_len;
static	int4	status;

	if  (!lock_used && boolex)
		rts_error(VARLSTCNT(1) ERR_ZLKIDBADARG);

	if  (!boolex)  lock_id = -1;
	item_list.itmcode = LKI$_LOCKID;
	item_list.bufflen = 4;
	item_list.buffaddr = &out_value;
	item_list.retlen = &out_len;
	item_list.end = 0;
	if  ((status = sys$getlkiw (0, &lock_id, &item_list, 0, 0, 0, 0)) == SS$_NORMAL)
	{
		i2mval(retval,out_value) ;
		lock_used = TRUE;
	}
	else
	{
		if  (status != SS$_NOMORELOCK)  rts_error(VARLSTCNT(1) status);
		retval->mvtype = MV_STR;
		retval->str.addr = 0;
		retval->str.len = 0;
		lock_used = FALSE;
	}
}
