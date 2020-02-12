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

#include "compiler.h"
#include "mint2mval.h"
#include "mvalconv.h"

void	mint2mval(int src, int this_bool_depth, mval *ret)
{
	/* Note: `this_bool_depth` parameter is unused for now */
	i2mval(ret, src);
}
