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
#include "gtm_stdio.h"
#ifdef __MVS__
#include <signal.h>
#else
#include <sys/signal.h>
#endif
#include "cli.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_stop.h"

void mupip_stop(void)
{
	int	process_id;
	unsigned short len, temp;
	char	buff[256];
	error_def(ERR_MUPCLIERR);

	len = sizeof(buff);
	if (!cli_get_str("ID",buff,&len))
		mupip_exit(ERR_MUPCLIERR);
	temp = len;
	process_id = asc2i((uchar_ptr_t)buff,len);
	if (process_id < 0)
	{	util_out_print("Error converting !AD to a number", TRUE, len, buff);
		mupip_exit(ERR_MUPCLIERR);
	}
	else
	{
		if (kill(process_id,SIGTERM) == -1)
		{	util_out_print("Error issuing STOP to process !UL",TRUE,process_id);
			PERROR("stop error");
		}else
		{	util_out_print("STOP issued to process !UL",TRUE,process_id);
		}
	}
	return;
}
