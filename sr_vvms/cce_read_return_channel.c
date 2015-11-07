/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "ccp.h"
#include <iodef.h>
#include <ssdef.h>
#include "efn.h"
#include "cce_output.h"

GBLREF unsigned short cce_return_channel;

void cce_read_return_channel(void)
{
	unsigned short mbsb[4];
	uint4 status;
	unsigned char buff[255];
	int4	efn_mask;
	static uint4 time[2] = { -30000000, -1 };  /* 3 seconds */
	error_def(ERR_CCERDTIMOUT);

	cce_out_open();
	for (;;)
	{
		efn_mask = (1 << efn_immed_wait | 1 << efn_timer);
		status = sys$setimr(efn_timer,time,0,&cce_return_channel,0);
		if ((status & 1) == 0)
			lib$signal(status);
		status = sys$qio(efn_immed_wait, cce_return_channel, IO$_READVBLK, &mbsb[0], 0, 0, &buff,
							SIZEOF(buff), 0, 0, 0, 0);
		if ((status & 1) == 0)
			lib$signal(status);
		if ((status = sys$wflor(efn_immed_wait,efn_mask)) != SS$_NORMAL)
			lib$signal(status);
		if ((status = sys$readef(efn_immed_wait, &efn_mask)) == SS$_WASSET)
		{	sys$cantim(&cce_return_channel,0);
	 		status = mbsb[0];
			if (status == SS$_ENDOFFILE)
				break;
			if ((status & 1) == 0)
				lib$signal(status);
			cce_out_write(buff, mbsb[1]);
		}else
		{	lib$signal(ERR_CCERDTIMOUT);
			break;
		}
	}
	cce_out_close();
	return;
}
