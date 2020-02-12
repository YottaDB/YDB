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
#include "bool_finish.h"
#include "bool_zysqlnull.h"

void	bool_finish(void)
{
	assert(NULL != boolZysqlnull);
	assert(boolZysqlnull->booleval_in_prog);
	bool_zysqlnull_finish();
	return;
}
