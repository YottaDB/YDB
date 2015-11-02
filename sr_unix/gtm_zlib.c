/****************************************************************
 *								*
 *	Copyright 2008, 2011 Fidelity Information Services, Inc	*
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

void gtm_zlib_init(void)
{
	char		*libname, err_msg[MAX_ERRSTR_LEN];
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

	error_def(ERR_DLLNOOPEN);
	error_def(ERR_TEXT);
	error_def(ERR_DLLNORTN);

	assert(gtm_zlib_cmp_level);
	libname = ZLIB_LIBNAME;
	handle = dlopen(libname, ZLIB_LIBFLAGS);
	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, err_msg);
		gtm_putmsg(VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(libname), ERR_TEXT, 2, LEN_AND_STR(err_msg));
		gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
		return;
	}
	for (findx = 0; findx < ZLIB_NUM_DLSYMS; ++findx)
	{
		fptr = (void *)dlsym(handle, zlib_fname[findx]);
		if (NULL == fptr)
		{
			COPY_DLLERR_MSG(err_str, err_msg);
			gtm_putmsg(VARLSTCNT(8) ERR_DLLNORTN, 2, LEN_AND_STR(zlib_fname[findx]), ERR_TEXT, 2, LEN_AND_STR(err_str));
			gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
			return;
		}
		*zlib_fptr[findx] = fptr;
	}
	return;
}
