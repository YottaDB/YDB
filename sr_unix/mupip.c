/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"

<<<<<<< HEAD
int main(int argc, char **argv, char **envp)
=======
#ifdef UTF8_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF	int			(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF	bool			in_backup;
GBLREF	bool			licensed;
GBLREF	int			(*func)();
GBLREF	spdesc			rts_stringpool, stringpool;
GBLREF	char			cli_err_str[];
GBLREF	CLI_ENTRY		mupip_cmd_ary[];
GBLREF	void			(*mupip_exit_fp)(int errcode);

GBLDEF	CLI_ENTRY		*cmd_ary = &mupip_cmd_ary[0];	/* Define cmd_ary to be the MUPIP specific cmd table */

void display_prompt(void);

int main (int argc, char **argv)
>>>>>>> 52a92dfd (GT.M V7.0-001)
{
	return dlopen_libyottadb(argc, argv, envp, "mupip_main");
}
