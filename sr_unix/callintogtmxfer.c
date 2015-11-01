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

typedef	int	(*int_fptr)();

int	hiber_start();
int	hiber_start_wait_any();
int	start_timer();
int	cancel_timer();
GBLREF	int	jnlpool_detach();

GBLDEF int (*callintogtm_vectortable[])()=
{
	hiber_start,
	hiber_start_wait_any,
	start_timer,
	cancel_timer,
	(int_fptr)gtm_malloc,
	(int_fptr)gtm_free,
	jnlpool_detach,
	(int_fptr)(-1)
};

