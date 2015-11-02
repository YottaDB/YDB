/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include <setjmp.h>
#include "invocation_mode.h"
#include "gtmimagename.h"
#include "error.h"

GBLREF enum gtmImageTypes	image_type;

void err_init(void (*x)())
{ 	/* Due to nested callins, we allocate bigger condition handlers stack for mumps and other trigger capable
	   images than for non-trigger capable utilities */

	int size;

	switch(image_type)
	{
		case GTM_IMAGE:
#ifdef GTM_TRIGGER
		case MUPIP_IMAGE:
		case GTCM_SERVER_IMAGE:
		case GTCM_GNP_SERVER_IMAGE:
#endif
			size = MAX_MUMPS_HANDLERS;
			break;
		default:
			size = MAX_HANDLERS;
	}
	chnd = (condition_handler*)malloc(size * SIZEOF(condition_handler));
	chnd[0].ch_active = FALSE;
	active_ch = ctxt = &chnd[0];
	ctxt->ch = x;
	chnd_end = &chnd[size]; /* chnd_end is the end of the condition handler stack */
}
