/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#define BYPASS_MEMCPY_OVERRIDE	/* Signals gtm_string.h to not override memcpy(). This causes linking problems when libmumps.a
				 * is not available.
				 */
#include "main_pragma.h"

#undef UNIX		/* Causes non-YottaDB-runtime routines (libyottadb) to be used since libyottadb is not yet available */
#include "gtm_stdio.h"
#define UNIX
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"
#include <dlfcn.h>

typedef int (*exe_main_t)(int argc, char **argv, char **envp);

int dlopen_libyottadb(int argc, char **argv, char **envp, char *main_func)
{
	int		status;
	char		yottadb_file[GTM_PATH_MAX];
	char_ptr_t	fptr;
	int		dir_len;
	void_ptr_t	handle;
	exe_main_t	exe_main;
	/* We fake the output of the following messages to appear as if they were reported by GT.M error
	 * handlers (gtm_putmsg/rts_error) by prefixing every message with %GTM-E-*
	 *   ERR_GTMDISTUNDEF
	 *   ERR_DISTPATHMAX
	 *   ERR_DLLNOOPEN
	 *   ERR_DLLNORTN
	 */

	if (!(fptr = (char_ptr_t)GETENV(GTM_DIST)))
	{
		FPRINTF(stderr, "%%GTM-E-GTMDISTUNDEF, Environment variable $gtm_dist is not defined\n");
		return ERR_GTMDISTUNDEF;
	}
	dir_len = STRLEN(fptr);
	if (GTM_DIST_PATH_MAX <= dir_len)
	{
		FPRINTF(stderr, "%%GTM-E-DISTPATHMAX, $gtm_dist path is greater than maximum (%d)\n",
			GTM_DIST_PATH_MAX);
		return ERR_DISTPATHMAX;
	}
	memcpy(&yottadb_file[0], fptr, dir_len);
	yottadb_file[dir_len] = DIR_SEPARATOR;
	memcpy(&yottadb_file[dir_len + 1], YOTTADB_IMAGE_NAME, STR_LIT_LEN(YOTTADB_IMAGE_NAME));
	yottadb_file[dir_len + STR_LIT_LEN(YOTTADB_IMAGE_NAME) + 1] = 0;

	/* RTLD_NOW - resolve immediately so we know errors sooner than later
	 * RTLD_GLOBAL - make all exported symbols from libyottadb.so available for subsequent dlopen.
	 */
	handle = dlopen(&yottadb_file[0], (RTLD_NOW | RTLD_GLOBAL));
	if (NULL == handle)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNOOPEN, Failed to load external dynamic library %s\n", yottadb_file);
		if ((fptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", fptr);
		return ERR_DLLNOOPEN;
	}
	status = 0;
	exe_main = (exe_main_t)dlsym(handle, main_func);
	if (NULL == exe_main)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNORTN, Failed to look up the location of the symbol %s\n", main_func);
		if ((fptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", fptr);
		return ERR_DLLNORTN;
	}
	status = exe_main(argc, argv, envp);
	return status;
}
