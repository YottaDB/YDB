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

#include "gtm_string.h"
#include "gtm_limits.h"

#include <dlfcn.h>

#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */
#include "gtmmsg.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "real_len.h"
#include "gtmcrypt.h"

#ifndef	GTM_DIST
#define GTM_DIST	"gtm_dist"
#endif

GBLREF	char	dl_err[MAX_ERRSTR_LEN];
GBLREF	int4	gbl_encryption_ecode;

void gtmcrypt_entry()
{
	char		*libname, err_msg[MAX_ERRSTR_LEN];
	void_ptr_t	handle;
	char_ptr_t	err_str;
	char		*gtmcryptlib_fname[] = {
				GTMCRYPT_INIT_FNAME,
				GTMCRYPT_CLOSE_FNAME,
				GTMCRYPT_HASH_GEN_FNAME,
				GTMCRYPT_ENCODE_FNAME,
				GTMCRYPT_DECODE_FNAME,
				GTMCRYPT_GETKEY_BY_NAME,
				GTMCRYPT_GETKEY_BY_HASH,
				GTMCRYPT_STRERROR
			};
	void		**gtmcryptlib_fptr[] = {
				(void **)&gtmcrypt_init_fnptr,
				(void **)&gtmcrypt_close_fnptr,
				(void **)&gtmcrypt_hash_gen_fnptr,
				(void **)&gtmcrypt_encode_fnptr,
				(void **)&gtmcrypt_decode_fnptr,
				(void **)&gtmcrypt_getkey_by_name_fnptr,
				(void **)&gtmcrypt_getkey_by_hash_fnptr,
				(void **)&gtmcrypt_strerror_fnptr
			};
	int		findx;
	int		num_dlsyms = SIZEOF(gtmcryptlib_fptr) / SIZEOF(gtmcryptlib_fptr[0]); /* number of functions in library */
	void		*fptr;
	char		*gtm_dist_path;
	char		libpath[GTM_PATH_MAX];

	error_def(ERR_CRYPTDLNOOPEN);
	error_def(ERR_GTMDISTUNDEF);
	if (NULL == (gtm_dist_path = getenv(GTM_DIST)))
		rts_error(VARLSTCNT(1) ERR_GTMDISTUNDEF);

	memset(libpath, 0, GTM_PATH_MAX);
	SNPRINTF(libpath, GTM_PATH_MAX, "%s/%s/%s", gtm_dist_path, "plugin", GTMCRYPT_LIBNAME);

	gbl_encryption_ecode = 0;
	handle = dlopen(libpath, GTMCRYPT_LIBFLAGS);
	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, dl_err);
		gbl_encryption_ecode = ERR_CRYPTDLNOOPEN; /* library initialization failed */
		return;
	}

	for(findx = 0; findx < num_dlsyms; ++findx)
	{
		fptr = (void *)dlsym(handle, gtmcryptlib_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, dl_err);
			gbl_encryption_ecode = ERR_CRYPTDLNOOPEN; /* library initialization failed */
			return;
		}
		*gtmcryptlib_fptr[findx] = fptr;
	}
	return;
}
