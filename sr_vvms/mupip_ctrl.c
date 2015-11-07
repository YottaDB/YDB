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
#include <ssdef.h>
#include "mupip_ctrl.h"

#define CTRLC     3
#define CTRLY	  25

GBLDEF bool	mu_ctrly_occurred;
GBLDEF bool	mu_ctrlc_occurred;

void mupip_ctrl(int4 ob_char)
{

	if (ob_char == CTRLC)
	{	mu_ctrlc_occurred = TRUE;
	}else if ( ob_char == CTRLY)
	{	mu_ctrly_occurred = TRUE;
	}else
	{
		GTMASSERT;
	}
}
