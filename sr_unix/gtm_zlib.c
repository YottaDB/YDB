/****************************************************************
 *								*
 * Copyright (c) 2008-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <dlfcn.h>
#include "gtm_string.h"

#include "real_len.h"		/* for COPY_DLERR_MSG */
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */
#include "gtm_zlib.h"
#include "gtmmsg.h"

error_def(ERR_DLLNOOPEN);
error_def(ERR_TEXT);
error_def(ERR_DLLNORTN);

void gtm_zlib_init(void)
{
	char		*libname, err_msg[MAX_ERRSTR_LEN];
#ifdef _AIX
	char		aix_err_msg[MAX_ERRSTR_LEN];
#endif
	void_ptr_t	handle;
	char_ptr_t	err_str;
	char		*zlib_fname[] = {
				ZLIB_CMP_FNAME,
				ZLIB_UNCMP_FNAME,
			};
	void		**zlib_fptr[] = {
				(void **)&zlib_compress_fnptr,
				(void **)&zlib_uncompress_fnptr,
			};
	int		findx;
	void		*fptr;

	assert(gtm_zlib_cmp_level);
#ifdef _AIX
	/* Attempt to load the AIX packaged zlib first */
	if (NULL == (handle = dlopen(ZLIB_AIXLIBNAME, ZLIB_LIBFLAGS | RTLD_MEMBER))) /* inline assignment */
	{
		COPY_DLLERR_MSG(err_str, aix_err_msg);
#endif
		libname = ZLIB_LIBNAME;
		handle = dlopen(libname, ZLIB_LIBFLAGS);
		if (NULL == handle)
		{
			COPY_DLLERR_MSG(err_str, err_msg);
#		ifdef _AIX
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname),
					ERR_TEXT, 2, LEN_AND_STR(err_msg),
					ERR_TEXT, 2, LEN_AND_STR(aix_err_msg));
#		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname),
					ERR_TEXT, 2, LEN_AND_STR(err_msg));
#		endif
			gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
			return;
		}
#ifdef _AIX
	}
#endif
	for (findx = 0; findx < ZLIB_NUM_DLSYMS; ++findx)
	{
		fptr = (void *)dlsym(handle, zlib_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, err_msg);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DLLNORTN, 2, LEN_AND_STR(zlib_fname[findx]),
					ERR_TEXT, 2, LEN_AND_STR(err_str));
			gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
			return;
		}
		*zlib_fptr[findx] = fptr;
	}
	return;
}
