/****************************************************************
 *								*
<<<<<<< HEAD:sr_unix_cm/gtcm_server.c
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 7a1d2b3e... GT.M V6.3-007:sr_unix/geteuid.c
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtcm.h"
#include "cli.h"

<<<<<<< HEAD:sr_unix_cm/gtcm_server.c
int main(int argc, char **argv, char **envp)
=======
#include <sys/types.h>

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "cli.h"

/* This executable does not have any command tables so initialize command array to NULL.
 * (op_zcompile etc. require cmd_ary).
 */
GBLDEF  CLI_ENTRY       *cmd_ary = NULL;

int main(int argc, char **argv)
>>>>>>> 7a1d2b3e... GT.M V6.3-007:sr_unix/geteuid.c
{
	return dlopen_libyottadb(argc, argv, envp, "gtcm_server_main");
}
