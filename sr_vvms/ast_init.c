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
#include <jpidef.h>
#include "ast_init.h"

#define MIN_DYN_ASTS 4

GBLDEF short astq_dyn_avail;
GBLDEF short astq_dyn_alloc;
GBLDEF short astq_dyn_min;

void ast_init(void)
{
int4 item, outv;

	item = JPI$_ASTLM;
	lib$getjpi(&item, 0, 0, &outv, 0, 0);
	astq_dyn_alloc = astq_dyn_avail = outv;
	astq_dyn_min = MIN_DYN_ASTS;
	sys$setrwm(0);	/* set resource wait mode, so will not create error if exceeded I/O limit */
}
