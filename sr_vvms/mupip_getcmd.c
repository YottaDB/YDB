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
#include <rmsdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <ssdef.h>
#include "mupip_exit.h"

#ifdef 	IPCRM_FOR_SANCHEZ_ONLY
#define	CMD_MODULE	IPCRM_CMD
#else
#define	CMD_MODULE	MUPIP_CMD
#endif


extern int	MUPIP_CMD(), IPCRM_CMD(),
		CLI$DCL_PARSE(), CLI$DISPATCH();

void mupip_getcmd(void)
{
	char buff[512];
	$DESCRIPTOR(command,buff);

	int		status;
	unsigned short outlen;
	unsigned char	buf[256];
	$DESCRIPTOR	(action, buf);
	$DESCRIPTOR	(prompt,"MUPIP> ");
	$DESCRIPTOR	(mupip_action, "MUPIP_ACTION");

	status = lib$get_foreign(&command,0,&outlen,0);
	if ((status & 1) && outlen > 0)
	{	command.dsc$w_length = outlen;
		status = CLI$DCL_PARSE(&command ,&CMD_MODULE, &lib$get_input, 0, 0);
		if (status == CLI$_NORMAL)
			CLI$DISPATCH();
	}else
	{	for (;;)
		{
			status = CLI$DCL_PARSE (0, &CMD_MODULE,
						&lib$get_input, &lib$get_input,
						&prompt);
			if (status == RMS$_EOF)
				break;
			if (status == CLI$_NORMAL)
				CLI$DISPATCH ();
		}
	}
/*****************REVERT TO DCL COMMAND TABLE CODE**********************
	status = CLI$GET_VALUE (&mupip_action, &action, 0);
	if (status == CLI$_ABSENT)
		for (;;)
		{
			status = CLI$DCL_PARSE (0, &CMD_MODULE,
						&lib$get_input, &lib$get_input,
						&prompt);
			if (status == RMS$_EOF)
				mupip_exit(SS$_NORMAL);
			if (status == CLI$_NORMAL)
				CLI$DISPATCH ();
		}
	else
		mupip_dispatch (&action);
**************************************************************************/
	mupip_exit(SS$_NORMAL);
}
