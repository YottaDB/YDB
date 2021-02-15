/****************************************************************
 *								*
<<<<<<< HEAD:sr_unix_cm/gtcm_server.c
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 451ab477 (GT.M V7.0-000):sr_unix/gvcmz_bunch_stub.c
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
{
	return dlopen_libyottadb(argc, argv, envp, "gtcm_server_main");
=======
error_def(ERR_UNIMPLOP);

void gvcmz_bunch(mval *v)
{
	RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
>>>>>>> 451ab477 (GT.M V7.0-000):sr_unix/gvcmz_bunch_stub.c
}
