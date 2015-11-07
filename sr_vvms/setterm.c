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
#include "setterm.h"

GBLREF int4		outofband;

void resetterm(io_desc *iod)
{
	short		iosb[4];
	uint4 	status;
	d_tt_struct     *tt_ptr;

	assert(iod->type == tt);
	if (outofband)
	{
		outofband_action(FALSE);
		assert(FALSE);
	}
	tt_ptr = (d_tt_struct *) iod->dev_sp;
	status = sys$qiow(EFN$C_ENF, tt_ptr->channel ,(IO$_SETMODE | IO$M_OUTBAND) ,iosb ,NULL ,0 ,0 ,0 ,0 ,0 ,0 ,0);
	if (status == SS$_NORMAL)
		status = iosb[0];
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
}

void setterm(io_desc *iod)
{
	uint4 	disable_msk, dummy_msk, status;

	assert(iod->type == tt);
	disable_msk = CTRLY_MSK;
	status = lib$disable_ctrl(&disable_msk, &dummy_msk);
	if (status != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
	iott_resetast(iod);
}
