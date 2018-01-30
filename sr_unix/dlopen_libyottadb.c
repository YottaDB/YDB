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
#include "ydb_chk_dist.h"

#ifdef DEBUG
# include <sys/types.h>	/* needed by "assert" macro */
# include <signal.h>	/* needed by "assert" macro */
# include <unistd.h>	/* needed by "assert" macro */
#endif

/* Define "assert" macro (cannot use normal assert as that needs "rts_error_csa" which we don't want to pull in here) */
#undef assert	/* in case it is defined */
#ifdef DEBUG
# define assert(x) ((x) ? 1 : (fprintf(stderr, "Assert failed at %s line %d : %s\n", __FILE__, __LINE__, #x), kill(getpid(), SIGILL)))
#else
# define assert(x)
#endif

typedef int (*exe_main_t)(int argc, char **argv, char **envp);

int dlopen_libyottadb(int argc, char **argv, char **envp, char *main_func)
{
	int		status, pathlen;
	char		*pathptr, *tmpptr;	/* Includes null terminator char */
	char		curr_exe_realpath[YDB_PATH_MAX], libyottadb_realpath[YDB_PATH_MAX];
	void_ptr_t	handle;
	exe_main_t	exe_main;

	/* We fake the output of the following messages to appear as if they were reported by YottaDB error
	 * handlers (gtm_putmsg/rts_error) by prefixing every message with %YDB-E-*
	 *   ERR_YDBDISTUNDEF
	 *   ERR_DISTPATHMAX
	 *   ERR_DLLNOOPEN
	 *   ERR_DLLNORTN
	 */
	/* Determine the path of the executable and use that to set the environment variable "ydb_dist"
	 * (and "gtm_dist", supported for backward compatibility). Note that we cannot use argv[0] in case
	 * the executable was invoked without specifying an absolute or relative path (e.g. using $PATH in shell).
	 */
	/* Get currently running executable */
	pathptr = realpath(PROCSELF, curr_exe_realpath);
	if (NULL != pathptr)
	{
		pathlen = STRLEN(pathptr);
		assert(DIR_SEPARATOR == pathptr[0]);
		assert(pathlen);
		tmpptr = pathptr + pathlen - 1;
		while (DIR_SEPARATOR != *tmpptr)
			tmpptr--;
		assert(tmpptr >= pathptr);
		/* At this point "tmpptr" points to the last '/' */
		/* At this point "pathptr" points to the pathname. Now check if PATH + "/libyottadb.so" can fit in YDB_PATH_MAX */
		if (YDB_DIST_PATH_MAX < ((tmpptr - pathptr) + STR_LIT_LEN(DIR_SEPARATOR) + STR_LIT_LEN(YOTTADB_IMAGE_NAME) + 1))
			pathptr = NULL;	/* so we issue a DISTPATHMAX error below */
	}
	if (NULL == pathptr)
	{
		FPRINTF(stderr, "%%GTM-E-DISTPATHMAX, Executable path length is greater than maximum (%d)\n", YDB_DIST_PATH_MAX);
		return ERR_DISTPATHMAX;
	}
	/* Now set "ydb_dist" (and "gtm_dist") to the obtained canonical path. "pathptr" points to that. */
	assert(DIR_SEPARATOR == *tmpptr);
	*tmpptr = '\0';
	setenv(YDB_DIST, pathptr, TRUE);
	setenv("gtm_dist", pathptr, TRUE);
	/* Get curr_exe_realpath back to its correct form (with the executable name at the end) */
	*tmpptr++ = DIR_SEPARATOR;
	/* Now open libyottadb.so */
	assert(SIZEOF(libyottadb_realpath) == SIZEOF(curr_exe_realpath));
	memcpy(libyottadb_realpath, curr_exe_realpath, tmpptr - curr_exe_realpath);
	tmpptr = libyottadb_realpath + (tmpptr - curr_exe_realpath);
	memcpy(tmpptr, YOTTADB_IMAGE_NAME, STR_LIT_LEN(YOTTADB_IMAGE_NAME));
	tmpptr[STR_LIT_LEN(YOTTADB_IMAGE_NAME)] = 0;

	/* RTLD_NOW - resolve immediately so we know errors sooner than later
	 * RTLD_GLOBAL - make all exported symbols from libyottadb.so available for subsequent dlopen.
	 */
	handle = dlopen(&libyottadb_realpath[0], (RTLD_NOW | RTLD_GLOBAL));
	if (NULL == handle)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNOOPEN, Failed to load external dynamic library %s\n", libyottadb_realpath);
		if ((tmpptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", tmpptr);
		return ERR_DLLNOOPEN;
	}
	status = 0;
	exe_main = (exe_main_t)dlsym(handle, main_func);
	if (NULL == exe_main)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNORTN, Failed to look up the location of the symbol %s\n", main_func);
		if ((tmpptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", tmpptr);
		return ERR_DLLNORTN;
	}
	/* Switch argv[0] to canonical full path of executable returned by the "realpath" call above.
	 * When argv[0] is later used in "ydb_chk_dist", we need the canonical path to avoid incorrect YDBDISTUNVERIF errors.
	 */
	argv[0] = curr_exe_realpath;
	status = exe_main(argc, argv, envp);
	return status;
}
