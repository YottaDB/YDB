/****************************************************************
 *								*
 * Copyright 2009, 2014 Fidelity Information Services, Inc	*
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
#include "gtm_stdlib.h"
#include "mdef.h"
#include "gtmxc_types.h"
#include "gdsroot.h"
#include "is_file_identical.h"
#include "iosp.h"		/* for SS_NORMAL */

/* Checks whether the two fileids passed are identical  */
ydb_status_t gtm_is_file_identical(ydb_fileid_ptr_t fileid1, ydb_fileid_ptr_t fileid2)
{
	if (!fileid1 || (!fileid2))
		return FALSE;
	return is_gdid_identical((gd_id_ptr_t) fileid1, (gd_id_ptr_t) fileid2);
}

/* Converts given filename to unique file id. Uses filename_to_id internally for getting the unique id. Note that,
 * the allocation of the fileid structure is done here and the caller needs to worry only about free'ing the
 * allocated pointer via gtm_xcfileid_free.
 *
 * Note, a reimplementation of this routine exists in ydb_file_name_to_id() so any changes here need to be
 * reflected there.
 */
ydb_status_t gtm_filename_to_id(ydb_string_t *filename, ydb_fileid_ptr_t *fileid)
{
	boolean_t	status;
	gd_id_ptr_t	tmp_fileid;

	if (!filename)
		return FALSE;
	assert(fileid && !*fileid);
	tmp_fileid = (gd_id_ptr_t)malloc(SIZEOF(gd_id));
	status = (SS_NORMAL == filename_to_id(tmp_fileid, filename->address));
	*fileid = (ydb_fileid_ptr_t)tmp_fileid;
	return status;
}

/* Allocation of ydb_fileid_ptr_t happens in gtm_filename_to_id. During the close time, encryption library  needs to free these
 * externally allocated resources and this is done through this function. */
void gtm_xcfileid_free(ydb_fileid_ptr_t fileid)
{
	if (NULL != fileid)
		free(fileid);
}
