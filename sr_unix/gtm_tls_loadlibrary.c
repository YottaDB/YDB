/****************************************************************
 *								*
 * Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include "lv_val.h"
#include "real_len.h"
#include "fgncal.h"	/* needed for COPY_DLLERR_MSG() */
#include "gtmmsg.h"
#include "gtmcrypt.h"
#include "dlopen_handle_array.h"

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

	if (!ydb_dist_ok_to_use)
	{
		SNPRINTF(dl_err, MAX_ERRSTR_LEN, "%%YDB-E-YDBDISTUNVERIF, Environment variable $ydb_dist (%s) "
				"could not be verified against the executables path", ydb_dist);
		return -1;
	}
	SNPRINTF(libpath, YDB_PATH_MAX, "%s/%s/%s", ydb_dist, GTMCRYPT_PLUGIN_DIR_NAME, GTM_TLS_LIBNAME);
	if (NULL == (handle = dlopen(libpath, RTLD_GLOBAL | RTLD_NOW)))
	{
		COPY_DLLERR_MSG(err_str, dl_err);
		return -1;
	}
	dlopen_handle_array_add(handle);
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
