/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2020 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "zshow.h"

GBLREF spdesc stringpool;

/* WARNING!!! - the it is left to the caller of this routine to protect the stringpool if appropriate */
void mval_lex(mval *v, mstr *output)
{
	int space_needed, des_len;

	MV_FORCE_STR(v);
	if (MV_IS_CANONICAL(v))
		*output = v->str;
	else if (MV_IS_SQLNULL(v))
	{
		ENSURE_STP_FREE_SPACE(DOLLAR_ZYSQLNULL_STRLEN);
		output->addr = (char *)stringpool.free;
		MEMCPY_LIT(stringpool.free, DOLLAR_ZYSQLNULL_STRING);
		output->addr = (char *)stringpool.free;
		output->len = DOLLAR_ZYSQLNULL_STRLEN;
	} else
	{
		space_needed = ZWR_EXP_RATIO(v->str.len);
		ENSURE_STP_FREE_SPACE(space_needed);
		output->addr = (char *)stringpool.free;
		des_len = space_needed;
		format2zwr((sm_uc_ptr_t)v->str.addr, v->str.len, (unsigned char *)output->addr, &des_len);
		output->len = des_len; /* need a temporary des_len since output->len is short on the VAX
					* and format2zwr expects an (int *) as the last parameter */
		assert(space_needed >= output->len);
	}
}
