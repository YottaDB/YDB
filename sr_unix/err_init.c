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
#include <setjmp.h>

/* Force defined values in error.h to expand as GBLDEF. */
#define CHEXPAND
#include "error.h"

void err_init(void (*x)())
{
	chnd[0].ch_active = FALSE;
	active_ch = ctxt = &chnd[0];
	ctxt->ch = x;
}
