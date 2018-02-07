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

/* Simple YottaDB wrapper for the gtm_file_name_to_id() utility function */
ydb_status_t ydb_file_name_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid)
{
	return gtm_filename_to_id(filename, fileid);
}
