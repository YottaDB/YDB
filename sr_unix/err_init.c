/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
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
{ /* Due to nested callins, we allocate bigger condition handlers stack for mumps than for
   * other utility programs (dse, mupip etc.) */
	int size;
	size = (GTM_IMAGE == image_type) ? MAX_MUMPS_HANDLERS : MAX_HANDLERS;
	chnd = (condition_handler*)malloc(size * sizeof(condition_handler));
	chnd[0].ch_active = FALSE;
	active_ch = ctxt = &chnd[0];
	ctxt->ch = x;
	chnd_end = &chnd[size]; /* chnd_end is the end of the condition handler stack */
}
