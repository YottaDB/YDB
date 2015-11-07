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

#include <ssdef.h>
#include <descrip.h>

#include "mdef.h"
#include "comp_esc.h"
#include "io.h"
#include "trans_log_name.h"


GBLDEF bool			ce_init_done = FALSE;
GBLDEF struct ce_sentinel_desc	*ce_def_list;


int ce_init (void)
{
	$DESCRIPTOR(filename,"GTM$COMPILER_ESCAPE");
	$DESCRIPTOR(entrypoint,"GTM$COMPILER_ESCAPE");

	int4	(*compiler_escape_init)();
	mstr	filename_logical, filename_translation;
	char	buffer[256];

	uint4	status;


	status = SS$_NORMAL;
	if (!ce_init_done)
	{
		ce_def_list = NULL;

		/* Check for existence of logical name; if present, invoke corresponding entry point. */
		filename_logical.addr = filename.dsc$a_pointer;
		filename_logical.len  = filename.dsc$w_length;

		if (trans_log_name(&filename_logical, &filename_translation, buffer) == SS$_NORMAL)
		{
			if ((status = lib$find_image_symbol(&filename, &entrypoint, &compiler_escape_init)) == SS$_NORMAL)
				compiler_escape_init();
		}

		ce_init_done = TRUE;
	}

	return status;
}
