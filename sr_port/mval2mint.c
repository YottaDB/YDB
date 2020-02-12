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
#include "mval2mint.h"
#include "mvalconv.h"

int4	mval2mint(mval *src, int this_bool_depth)
{
	/* Note: `this_bool_depth` parameter is unused for now */
	if (MV_IS_SQLNULL(src))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZYSQLNULLNOTVALID);
	return mval2i(src);
}
