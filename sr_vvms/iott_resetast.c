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
#include <efndef.h>

#include "io.h"
#include "iottdef.h"
#include "outofband.h"
#include "curr_dev_outbndset.h"
#include "std_dev_outbndset.h"

GBLREF io_pair 		io_std_device;
GBLREF uint4	std_dev_outofband_msk;

void iott_resetast( io_desc *io_ptr)
{
	void		std_dev_outbndset(), curr_dev_outbndset(), (*ast_routine)();
	short		iosb[4];
	uint4	status;
	d_tt_struct 	*tt_ptr;
	io_terminator	outofbands;

	tt_ptr = (d_tt_struct*) io_ptr->dev_sp;
	outofbands.x = 0;
	outofbands.mask = tt_ptr->enbld_outofbands.mask | tt_ptr->ctrlu_msk;

	if (io_ptr == io_std_device.in)
	{
		/* <CTRL-Y> doesn't do anything unless it's in the enabled_outofbands.mask,
		but we're using the std_dev_outofband_msk to hold the inital state of the CONTROL=Y;
		this probably deserves more attention, but for now, if we mask it out,
		the user at least gets to see evidence of disturbance e.g. that input was lost*/
		outofbands.mask |= (std_dev_outofband_msk & (~CTRLY_MSK));
		ast_routine = std_dev_outbndset;
	}
	else
		ast_routine = curr_dev_outbndset;

	status = sys$qiow(EFN$C_ENF, tt_ptr->channel
		,(IO$_SETMODE | IO$M_OUTBAND | IO$M_TT_ABORT) ,iosb
		,NULL ,0
		,ast_routine ,&outofbands ,0 ,0 ,0 ,0 );
	if (status == SS$_NORMAL)
		status = iosb[0];
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
}
