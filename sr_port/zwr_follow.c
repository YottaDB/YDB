/****************************************************************
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *                                                              *
 *      This source code contains the intellectual property     *
 *      of its copyright holder(s), and is made available       *
 *      under a license.  If you do not know the terms of       *
 *      the license, please stop and do not read further.       *
 *                                                              *
 ****************************************************************/

#include "mdef.h"

#include "zwr_follow.h"
#include "mmemory.h"

/* Returns 1 if `u` follows `v` and 0 otherwise.
 * `u` and `v` can be $ZYSQLNULL in which case it treats $ZYSQLNULL as following everything else except itself.
 * This is needed so one can ZWRITE a local variable. Currently globals cannot have $ZYSQLNULL as subscripts so
 * this functionality is not used by globals but will be eventually.
 */
boolean_t zwr_follow(mval *u, mval *v)
{
	long	result;

	if (MV_IS_SQLNULL(u))
	{
		if MV_IS_SQLNULL(v)
			return 0;	/* u=$ZYSQLNULL does NOT follow v=$ZYSQLNULL */
		else
			return 1;	/* u=$ZYSQLNULL does follow v!=$ZYSQLNULL */
	} else if (MV_IS_SQLNULL(v))
		return 0;		/* u!=$ZYSQLNULL does NOT follow v=$ZYSQLNULL */
	MV_FORCE_STR(u);
	MV_FORCE_STR(v);
	result = memvcmp(u->str.addr, u->str.len, v->str.addr, v->str.len);
	return (0 >= result) ? 0 : 1;
}
