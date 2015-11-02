/****************************************************************
 *								*
 *	Copyright 2008 Fidelity Information Services, Inc	*
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
#include "cli_disallow.h"
#include "lke_cmd_disallow.h"

GBLREF	char	*cli_err_str_ptr;

boolean_t cli_disallow_lke_clear(void)
{
	int disallow_return_value = 0;

	*cli_err_str_ptr = '\0';

	disallow_return_value = d_c_cli_present("EXACT") && !d_c_cli_present("LOCK");
	CLI_DIS_CHECK_N_RESET;
	return FALSE;
}

