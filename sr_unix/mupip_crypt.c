/****************************************************************
 *								*
 *	Copyright 2009, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"
#include "gtm_limits.h"
#include "cli.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_crypt.h"
#include "mu_decrypt.h"
#include "gtmcrypt.h"

void mupip_crypt(void)
{
#	ifdef GTM_CRYPT
	unsigned short		fname_len;
	char			fname[GTM_PATH_MAX];
	int4			len, off;

	error_def(ERR_MUPCLIERR);
	fname_len = SIZEOF(fname);
	if (!cli_get_str("FILE", fname, &fname_len))
		mupip_exit(ERR_MUPCLIERR);
	if (!cli_get_int("OFFSET", &off))
		mupip_exit(ERR_MUPCLIERR);
	if (!cli_get_int("LENGTH", &len))
		mupip_exit(ERR_MUPCLIERR);
	if (CLI_PRESENT == cli_present("DECRYPT"))
		mupip_exit(mu_decrypt(fname, off, len));
	else
		mupip_exit(ERR_MUPCLIERR);
#	endif
}
