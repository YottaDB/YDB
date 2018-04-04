/****************************************************************
 *								*
 * Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include <dlfcn.h>
#include <errno.h>
#ifdef _AIX
#include <sys/ldr.h>
#endif
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include "lv_val.h"
#include "real_len.h"
#include "fgncal.h"	/* needed for COPY_DLLERR_MSG() */
#include "gtmmsg.h"
#include "gtmcrypt.h"

typedef void (*gtm_tls_func_t)();	/* A generic pointer type to the TLS functions exposed by the plugin */

#define TLS_DEF(x) x##_,
enum
{
#include "gtm_tls_funclist.h"	/* BYPASSOK */
gtm_tls_func_n			/* total number of TLS functions that needs to be dlsym()ed */
};
#undef TLS_DEF

#define TLS_DEF(x) GBLDEF gtm_tls_func_t x##_fptr;
#include "gtm_tls_funclist.h"
#undef TLS_DEF

#define GTM_TLS_LIBNAME		"libgtmtls.so"

GBLREF	char		dl_err[MAX_ERRSTR_LEN];
GBLREF	char		ydb_dist[YDB_PATH_MAX];
GBLREF	boolean_t	ydb_dist_ok_to_use;

error_def(ERR_YDBDISTUNVERIF);

/* Cannot include gtm_tls.h in this module due to conflicting GBLDEF/GBLREFs. So, redefine the function prototype here to silent
 * the compiler.
 */
int	gtm_tls_loadlibrary(void);

int	gtm_tls_loadlibrary()
{
	/* Initialize the table of symbol names to be used in dlsym() */
#	define TLS_DEF(x) #x,
	char			*gtm_tls_fname[] = {
#							include "gtm_tls_funclist.h"
							NULL
						   };
#	undef TLS_DEF
	/* Initialize the table of locations of function pointers that are set by dlsym() */
	gtm_tls_func_t		fptr;
#	define TLS_DEF(x) &x##_fptr,
	gtm_tls_func_t		*gtm_tls_fptr[] = {
#							include "gtm_tls_funclist.h"
							NULL
						  };
#	undef TLS_DEF
	void_ptr_t		*handle;
	char			*err_str, libpath[YDB_PATH_MAX], util_libpath[YDB_PATH_MAX];
	int			findx;
#	ifdef _AIX
	char			new_libpath_env[YDB_PATH_MAX], *save_libpath_ptr, plugin_dir_path[YDB_PATH_MAX];
	char			save_libpath[YDB_PATH_MAX];
#	endif

	if(!ydb_dist_ok_to_use)
	{
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "%%YDB-E-YDBDISTUNVERIF, Environment variable $ydb_dist (%s) "
				"could not be verified against the executables path", ydb_dist);
		return -1;
	}
#	ifdef _AIX
	SNPRINTF(plugin_dir_path, YDB_PATH_MAX, "%s/%s", ydb_dist, GTMCRYPT_PLUGIN_DIR_NAME);
	SNPRINTF(libpath, YDB_PATH_MAX, "%s/%s/%s", ydb_dist, GTMCRYPT_PLUGIN_DIR_NAME, GTM_TLS_LIBNAME);
	/* Prefix LIBPATH with "$ydb_dist/plugin" so that dlopen can find the helper library (libgtmcryptutil.so). */
	if (NULL == (save_libpath_ptr = getenv(LIBPATH_ENV)))
		SNPRINTF(new_libpath_env, YDB_PATH_MAX, "%s", plugin_dir_path);
	else
	{
		/* Since the setenv below can potentially thrash the save_libpath_ptr, take a copy of it for later restore. */
		strncpy(save_libpath, save_libpath_ptr, SIZEOF(save_libpath));
		save_libpath[SIZEOF(save_libpath) - 1] = '\0';
		SNPRINTF(new_libpath_env, YDB_PATH_MAX, "%s:%s", plugin_dir_path, save_libpath);
	}
	setenv(LIBPATH_ENV, new_libpath_env, TRUE);
#	else
	SNPRINTF(libpath, YDB_PATH_MAX, "%s/%s/%s", ydb_dist, GTMCRYPT_PLUGIN_DIR_NAME, GTM_TLS_LIBNAME);
#	endif
	if (NULL == (handle = dlopen(libpath, RTLD_GLOBAL | RTLD_NOW)))
	{
		COPY_DLLERR_MSG(err_str, dl_err);
		return -1;
	}
#	ifdef _AIX
	/* Restore old LIBPATH. */
	if (NULL == save_libpath_ptr)
		unsetenv(LIBPATH_ENV);
	else
		setenv(LIBPATH_ENV, save_libpath, TRUE);
	/* Now verify that "libgtmcryptutil.so" was really loaded from "$ydb_dist/plugin". */
	if (!verify_lib_loadpath(GTMCRYPT_UTIL_LIBNAME, plugin_dir_path))
		return -1;
#	endif
	for (findx = 0; findx < gtm_tls_func_n; ++findx)
	{
		fptr = (gtm_tls_func_t)dlsym(handle, gtm_tls_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, dl_err);
			return -1;
		}
		*gtm_tls_fptr[findx] = fptr;
	}
	return 0;
}
