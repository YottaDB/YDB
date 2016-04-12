/****************************************************************
 *								*
 * Copyright (c) 2009-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_limits.h"
#include "gtm_string.h"

#include "cli.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_crypt.h"
#include "mu_decrypt.h"
#include "gtmcrypt.h"

error_def(ERR_MUPCLIERR);

void mupip_crypt(void)
{
	unsigned short		fname_len, type_len;
	char			fname[GTM_PATH_MAX], type[32];	/* Type should not be too long */
	int4			len, off;

	fname_len = SIZEOF(fname);
	cli_get_str("FILE", fname, &fname_len);
	cli_get_int("OFFSET", &off);
	cli_get_int("LENGTH", &len);
	if (CLI_PRESENT == cli_present("TYPE"))
	{
		type_len = SIZEOF(type);
		cli_get_str("TYPE", type, &type_len);
	} else
	{
		STRCPY(type, "DB_IV");
		type_len = STR_LIT_LEN("DB_IV");
	}
	mupip_exit(mu_decrypt(fname, fname_len, off, len, type, type_len));
}
