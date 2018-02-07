/****************************************************************
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtmxc_types.h"

/* Simple YottaDB wrapper for the gtm_is_file_identical() utility function */
ydb_status_t ydb_is_file_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2)
{
	return gtm_is_file_identical(fileid1, fileid2);
}
