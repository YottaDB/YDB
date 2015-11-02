/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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

#undef UNIX		/* Causes non-GTM-runtime routines (libgtmshr) to be used since libgtmshr is not yet available */
#include "gtm_stdio.h"
#define UNIX
#include "gtm_string.h"
#include "gtm_strings.h"
#include "gtm_stdlib.h"
#include "gtm_limits.h"
#include <dlfcn.h>
#ifdef __osf__
	/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
	 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
	 * the only exception and, in particular because the operating system does not support such an exception, the argv
	 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
	 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
	 */
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

#ifndef NOLIBGTMSHR
/* for bta builds we link gtm_main() statically so we do not need to open the shared library */
typedef int (*gtm_main_t)(int argc, char **argv, char **envp);
#ifdef __CYGWIN__
/* private copy of gtm_getenv needed since real one is in libgtmshr */
#include "gtm_unistd.h"

#ifdef GETENV
#undef GETENV
#endif
#define GETENV private_getenv

extern char **environ;		/* array of pointers, last has NULL */

char *private_getenv(const char *varname);

char *private_getenv(const char *varname)
{
	char	*eq, **p;
	size_t	len;

	if (NULL == environ || NULL == varname)
		return NULL;
	len = strlen(varname);
	for (p = environ; *p; p++)
	{
		eq = strchr(*p, '=');
		if (eq && (*p + len) == eq)
		{
			if (!strncasecmp(varname, *p, len))	/* gdb upcases names */
				return (eq + 1);
		}
	}
	return NULL;
}
#endif /* __CYGWIN__ */
#endif

int main (int argc, char **argv, char **envp)
#ifdef __osf__
#pragma pointer_size (restore)
#endif
{
	int		status;
#ifndef NOLIBGTMSHR
	char		gtmshr_file[GTM_PATH_MAX];
	char_ptr_t	fptr;
	int		dir_len;
	void_ptr_t	handle;
	gtm_main_t	gtm_main;
	/* We fake the output of the following messages to appear as if they were reported by GT.M error
	 * handlers (gtm_putmsg/rts_error) by prefixing every message with %GTM-E-* */
	int		ERR_GTMDISTUNDEF = 150377714;
	int		ERR_DISTPATHMAX = 150377682;
	int		ERR_DLLNOOPEN = 150379250;
	int		ERR_DLLNORTN = 150379258;

	if (!(fptr = (char_ptr_t)GETENV(GTM_DIST)))
	{
		FPRINTF(stderr, "%%GTM-E-GTMDISTUNDEF, Environment variable $gtm_dist is not defined\n");
		return ERR_GTMDISTUNDEF;
	}
	dir_len = STRLEN(fptr);
	if (GTM_PATH_MAX <= dir_len + STR_LIT_LEN(GTMSHR_IMAGE_NAME) + 1)
	{
		FPRINTF(stderr, "%%GTM-E-DISTPATHMAX, $gtm_dist path is greater than maximum (%lu)\n",
			(unsigned long)(GTM_PATH_MAX - STR_LIT_LEN(GTMSHR_IMAGE_NAME) - 2));
		return ERR_DISTPATHMAX;
	}
	memcpy(&gtmshr_file[0], fptr, dir_len);
	gtmshr_file[dir_len] = DIR_SEPARATOR;
	memcpy(&gtmshr_file[dir_len+1], GTMSHR_IMAGE_NAME, STR_LIT_LEN(GTMSHR_IMAGE_NAME));
	gtmshr_file[dir_len + STR_LIT_LEN(GTMSHR_IMAGE_NAME) + 1] = 0;

	/* RTLD_NOW - resolve immediately so we know errors sooner than later
	 * RTLD_GLOBAL - make all exported symbols from gtmshr available for subsequent dlopen */
	handle = dlopen(&gtmshr_file[0], (RTLD_NOW | RTLD_GLOBAL));
	if (NULL == handle)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNOOPEN, Failed to load external dynamic library %s\n", gtmshr_file);
		if ((fptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", fptr);
		return ERR_DLLNOOPEN;
	}
	gtm_main = (gtm_main_t)dlsym(handle, GTM_MAIN_FUNC);
	if (NULL == gtm_main)
	{
		FPRINTF(stderr, "%%GTM-E-DLLNORTN, Failed to look up the location of the symbol %s\n", GTM_MAIN_FUNC);
		if ((fptr = dlerror()) != NULL)
			FPRINTF(stderr, "%%GTM-E-TEXT, %s\n", fptr);
		return ERR_DLLNORTN;
	}
#endif
	status = gtm_main(argc, argv, envp);
	return status;
}
