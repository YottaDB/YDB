/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "comp_esc.h"


GBLDEF bool			ce_init_done = FALSE;
GBLDEF struct ce_sentinel_desc	*ce_def_list;


int ce_init (void)
{
	int4	(*compiler_escape_init)();
	mstr	filename_logical, filename_translation;
	char	buffer[256];

	uint4	status;


	status = 1;
	if (!ce_init_done)
	{
		ce_def_list = NULL;

		/* Check for existence of logical name; if present, invoke corresponding entry point. */
		/* No Unix code yet -- this is just a stub to initialize ce_def_list */

		ce_init_done = TRUE;
	}

	return status;
}
