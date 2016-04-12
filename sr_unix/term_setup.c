/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "io.h"
#include "term_setup.h"

GBLREF	boolean_t	ctrlc_on;	/* TRUE in cenable mode; FALSE in nocenable mode */
GBLREF	int4		outofband;	/*enumerated:ctrap,ctrlc or ctrly*/
GBLREF	io_pair		io_std_device;	/* standard device */

void  term_setup(boolean_t ctrlc_enable)
{
	outofband = 0;
	ctrlc_on = (io_std_device.in->type == tt) ? ctrlc_enable : FALSE;
}
