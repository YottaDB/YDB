/****************************************************************
 *								*
 * Copyright (c) 2008-2021 Fidelity National Information	*
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
#include "gtm_stdio.h"
#include "gtm_limits.h"

#include "real_len.h"		/* for COPY_DLERR_MSG */
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"		/* needed for COPY_DLLERR_MSG() */
#include "gtm_zlib.h"
#include "gtmmsg.h"
#include "send_msg.h"
#include "restrict.h"	/* Needed for restrictions */
#include "have_crit.h"	/* Needed for defer interrupts */

error_def(ERR_DLLNOOPEN);
error_def(ERR_DLLNORTN);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TEXT);

GBLREF char		gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t	gtm_dist_ok_to_use;

void gtm_zlib_init(void)
{
	char		err_msg[MAX_ERRSTR_LEN];
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
	char 		librarypath[GTM_PATH_MAX], *lpath = NULL;
	intrpt_state_t	prev_intrpt_state;

	assert(gtm_zlib_cmp_level);
#ifdef _AIX
	if (RESTRICTED(library_load_path))
	{
		lpath = librarypath;
		assert(gtm_dist_ok_to_use);
		SNPRINTF(librarypath, GTM_PATH_MAX, GTM_PLUGIN_FMT_SHORT ZLIB_AIXLIBNAME, gtm_dist);
	} else
		lpath = ZLIB_AIXLIBNAME;
	/* Attempt to load the AIX packaged zlib first */
	DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	handle = dlopen( lpath, ZLIB_LIBFLAGS | RTLD_MEMBER);
	ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
	if (NULL == handle)
	{
		COPY_DLLERR_MSG(err_str, aix_err_msg);
#endif
		if (RESTRICTED(library_load_path))
		{
			lpath = librarypath;
			SNPRINTF(librarypath, GTM_PATH_MAX, GTM_PLUGIN_FMT_SHORT ZLIB_LIBNAME, gtm_dist);
		} else
			lpath = ZLIB_LIBNAME;
		DEFER_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		handle = dlopen(lpath, ZLIB_LIBFLAGS);
		ENABLE_INTERRUPTS(INTRPT_IN_FUNC_WITH_MALLOC, prev_intrpt_state);
		if (NULL == handle)
		{
			if (RESTRICTED(library_load_path))
			{
				SNPRINTF(err_msg, MAX_ERRSTR_LEN, "dlopen(%s)", lpath);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, err_msg);
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, err_msg);
				gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
				return;
			}
			COPY_DLLERR_MSG(err_str, err_msg);
#		ifdef _AIX
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(12) ERR_DLLNOOPEN, 2, LEN_AND_STR(lpath),
					ERR_TEXT, 2, LEN_AND_STR(err_msg),
					ERR_TEXT, 2, LEN_AND_STR(aix_err_msg));
#		else
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_DLLNOOPEN, 2, LEN_AND_STR(lpath),
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
					ERR_TEXT, 2, LEN_AND_STR(err_msg));
			gtm_zlib_cmp_level = ZLIB_CMPLVL_NONE;	/* dont use compression */
			return;
		}
		*zlib_fptr[findx] = fptr;
	}
	return;
}
