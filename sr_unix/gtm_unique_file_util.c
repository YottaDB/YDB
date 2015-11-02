/****************************************************************
 *								*
 *	Copyright 2009 Fidelity Information Services, Inc	*
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

/* Checks whether the two fileids passed are identical  */
xc_status_t gtm_is_file_identical(xc_fileid_ptr_t fileid1, xc_fileid_ptr_t fileid2)
{
	if (!fileid1 || (!fileid2)) return FALSE;
	return is_gdid_identical((gd_id_ptr_t) fileid1, (gd_id_ptr_t) fileid2);
}

/* Converts given filename to unique file id. Uses filename_to_id internally for getting the unique id. Note that,
 * the allocation of the fileid structure is done here and the caller needs to worry only about free'ing the
 * allocated pointer via gtm_xcfileid_free. */
xc_status_t gtm_filename_to_id(xc_string_t *filename, xc_fileid_ptr_t *fileid)
{
	gd_id_ptr_t	tmp_fileid;
	boolean_t	status;

	if (!filename)
		return FALSE;
	assert(fileid && !*fileid);
	tmp_fileid = (gd_id_ptr_t)malloc(SIZEOF(gd_id));
	status = filename_to_id(tmp_fileid, filename->address);
	*fileid = (xc_fileid_ptr_t)tmp_fileid;
	return status;
}

/* Allocation of xc_fileid_ptr_t happens in gtm_filename_to_id. During the close time, encryption library  needs to free these
 * externally allocated resources and this is done through this function. */
void gtm_xcfileid_free(xc_fileid_ptr_t fileid)
{
	if (NULL != fileid)
		free(fileid);
}
