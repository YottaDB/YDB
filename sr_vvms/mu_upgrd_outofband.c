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
#include "efn.h"
#include "io.h"
#include "iottdef.h"
#include <iodef.h>
#include <dvidef.h>
#include <dcdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <efndef.h>
#include "mupip_ctrl.h"


#define OUTOFBAND_MSK 0x02000008


void mu_upgrd_outofband(void)
{	int4		status, channel, item_code, event;
	uint4	devclass;
	io_terminator	mu_outofband_msk;
	$DESCRIPTOR(sys_input,"SYS$INPUT");

	if ((status = sys$assign(&sys_input,&channel,0,0)) != SS$_NORMAL)
		rts_error(VARLSTCNT(1) status);
	item_code = DVI$_DEVCLASS;
	lib$getdvi(&item_code, &channel, 0, &devclass, 0, 0);
	if (devclass == DC$_TERM)
	{
		mu_outofband_msk.x = 0;
		mu_outofband_msk.mask = OUTOFBAND_MSK;
		if ((status = sys$qiow(EFN$C_ENF,channel
				,(IO$_SETMODE | IO$M_OUTBAND)
				,0 ,0 ,0
				,mupip_ctrl
				,&mu_outofband_msk
				,0 ,0 ,0 ,0 )) != SS$_NORMAL)
		{	rts_error(VARLSTCNT(1) status);
		}
		event = efn_outofband;
		status = sys$clref(event);
		if (status != SS$_WASSET && status != SS$_WASCLR)
		{	rts_error(VARLSTCNT(1) );
		}
	}
	return;
}

