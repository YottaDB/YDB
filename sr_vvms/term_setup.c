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
#include <iodef.h>
#include "io.h"
#include "iottdef.h"
#include "efn.h"
#include "outofband.h"
#include "term_setup.h"

GBLREF int4	    outofband;			/* enumerated:ctrap,ctrlc or ctrly */
GBLREF int4	    std_dev_outofband_msk;
GBLREF io_pair	    io_std_device;		/* standard device	*/

GBLDEF bool	    ctrlc_on;			/* whether ctrlc trap enabled */

void  term_setup(bool ctrlc_enable)

{
	uint4	status;
	io_terminator   outofbands;

	status = sys$clref(efn_outofband);
	assert(status == SS$_WASSET || status == SS$_WASCLR);
	outofband = 0;

	if (io_std_device.in->type == tt)
	{
		ctrlc_on = ctrlc_enable ;
		if (ctrlc_on)
			std_dev_outofband_msk |= CTRLC_MSK;
		iott_resetast(io_std_device.in);
	}else
		ctrlc_on = FALSE;
}
