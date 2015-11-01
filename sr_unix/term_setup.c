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
#include "io.h"
#include "term_setup.h"

GBLREF int4	    outofband;		/*enumerated:ctrap,ctrlc or ctrly*/
GBLREF io_pair	    io_std_device;	/* standard device	*/
GBLDEF bool	    ctrlc_on;		/* whether ctrlc trap enabled*/

void  term_setup(bool ctrlc_enable)

{
	if (io_std_device.in->type == tt)
	{
		outofband = 0;
		ctrlc_on = ctrlc_enable ;
	}
	else
	{
		outofband = 0;
		ctrlc_on = FALSE;
	}
}
