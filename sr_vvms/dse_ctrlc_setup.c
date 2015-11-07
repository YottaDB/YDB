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

/* chtest.c
/*
 */

#include "mdef.h"
#include "io.h"
#include "iottdef.h"
#include <iodef.h>
#include <dvidef.h>
#include <dcdef.h>
#include <ssdef.h>
#include <descrip.h>
#include <chfdef.h>
#include <efndef.h>
#include <signal.h>

GBLREF VSIG_ATOMIC_T	util_interrupt;

#define OUTOFBAND_MSK 0x02000008

void intr_handler(void)
{
    util_interrupt = 1;
}

void dse_ctrlc_setup(void)
{	int4		status, channel, item_code, event;
	uint4	devclass;
	io_terminator	outofband_msk;
	$DESCRIPTOR(sys_input,"SYS$INPUT");

	if ((status = sys$assign(&sys_input,&channel,0,0)) != SS$_NORMAL)
		lib$signal(status);
	item_code = DVI$_DEVCLASS;
	lib$getdvi(&item_code, &channel, 0, &devclass, 0, 0);
	if (devclass == DC$_TERM)
	{
		outofband_msk.x = 0;
		outofband_msk.mask = OUTOFBAND_MSK;
		if ((status = sys$qiow(EFN$C_ENF,channel
				,(IO$_SETMODE | IO$M_OUTBAND | IO$M_TT_ABORT)
				,0 ,0 ,0
				,intr_handler
				,&outofband_msk
				,0 ,0 ,0 ,0 )) != SS$_NORMAL)
		{	lib$signal(status);
		}
	}
	util_interrupt = 0;
	return;
}



