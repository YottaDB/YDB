/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, LLC.	*
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

#include "cli.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "error.h"
#include "mupip_exit.h"	/* Wrong name but does what we want */
#include "dbcertify.h"

GBLREF	phase_static_area	*psa_gbl;

extern int DBCERTIFY_CMD(), CLI$DCL_PARSE(), CLI$DISPATCH();

void dbcertify_parse_and_dispatch(int argc, char **argv)
{
	char 		buff[512];
	int		status;
	unsigned short	outlen;
	$DESCRIPTOR(command, buff);
	error_def(ERR_CLIERR);

	status = lib$get_foreign(&command, 0, &outlen, 0);
	if (status & 1)
	{
		if (0 < outlen)
		{
			command.dsc$w_length = outlen;
			status = CLI$DCL_PARSE(&command, &DBCERTIFY_CMD, &lib$get_input, 0, 0);
			if (status == CLI$_NORMAL)
			{	/* Before we dispatch the function, process one common parameter we need immediately */
				psa_gbl->dbc_debug = (CLI_PRESENT == cli_present("DEBUG"));
				CLI$DISPATCH();
			}
		} else
			rts_error(VARLSTCNT(4) ERR_CLIERR, 2, RTS_ERROR_LITERAL("No parameters specified"));
	}
	mupip_exit(status);
}
