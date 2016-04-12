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

#include "gtm_stdio.h"
#include "gtm_string.h"

#include "cli.h"
#include "cli_parse.h"
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
#include "iosp.h"
#include "error.h"
#include "dbcertify.h"

GBLREF	phase_static_area	*psa_gbl;			/* Global anchor for static area */
GBLREF	char			cli_err_str[];
GBLREF	void			(*func)(void);

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif
void dbcertify_parse_and_dispatch(int argc, char **argv)
#ifdef __osf__
#pragma pointer_size (restore)
#endif
{
	int	res;

	error_def(ERR_CLIERR);

	/* Simple check for missing parameters */
	if (1 >= argc)
		rts_error(VARLSTCNT(4) ERR_CLIERR, 2, RTS_ERROR_LITERAL("No parameters specified"));
	cli_lex_setup(argc, argv);

	if (EOF == (res = parse_cmd()))
		rts_error(VARLSTCNT(4) ERR_CLIERR, 2, RTS_ERROR_LITERAL("No parameters specified"));
	else if (res)
		rts_error(VARLSTCNT(4) res, 2, LEN_AND_STR(cli_err_str));
	if (func)
	{
		/* Before we dispatch the function, process one common parameter we need immediately */
		psa_gbl->dbc_debug = (CLI_PRESENT == cli_present("DEBUG"));
		func();
	} else
		GTMASSERT;
}
