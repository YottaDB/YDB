/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

/* Allocate a basic initial condition handler stack that can be expanded later if necessary */
void err_init(void (*x)())
{
	chnd = (condition_handler *)malloc((CONDSTK_INITIAL_INCR + CONDSTK_RESERVE) * SIZEOF(condition_handler));
	chnd[0].ch_active = FALSE;
	chnd[0].save_active_ch = NULL;
	active_ch = ctxt = &chnd[0];
	ctxt->ch = x;
	chnd_end = &chnd[CONDSTK_INITIAL_INCR]; /* chnd_end is the end of the condition handler stack */
	chnd_incr = CONDSTK_INITIAL_INCR * 2;
}
