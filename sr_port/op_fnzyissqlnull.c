/****************************************************************
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

#include "op.h"

LITREF mval *literal_zero_one_table[2];

/* `str` is input string that is checked to see if it is $ZYSQLNULL or not.
 * `ret` is return string that is set to `&literal_one` if `str` is $ZYSQLNULL (i.e. has the MV_IS_SQLNULL bit set)
 *	and set to `&literal_zero` otherwise.
 */
void op_fnzyissqlnull(mval *str, mval *ret)
{
	boolean_t	issqlnull;

	issqlnull = (0 != MV_IS_SQLNULL(str));
	*ret = *literal_zero_one_table[issqlnull];
}
