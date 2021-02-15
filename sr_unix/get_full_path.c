/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "rmv_mul_slsh.h"

#include <sys/param.h>
#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "have_crit.h"

error_def(ERR_FILENAMETOOLONG);

/* Gets the full path name for a given file name. Prepends the CWD, even if the file does not exist. */
boolean_t get_full_path(char *orig_fn, unsigned int orig_len, char *full_fn, unsigned int *full_len,
		unsigned int max_len, uint4 *status)
{
<<<<<<< HEAD
	char	*cptr, *c1;
	char	cwdbuf[YDB_PATH_MAX];
	int	cwd_len;
	int	i, length;
	char	*getcwd_res;
=======
	char		*cptr, *c1;
	char		cwdbuf[GTM_PATH_MAX];
	unsigned int	cwd_len, dir_len, newfn_len, trim_len;
	int		i;
	char		*getcwd_res;
	unsigned int	length;
>>>>>>> 451ab477 (GT.M V7.0-000)

	if ('/' == *orig_fn)
	{	/* The original path is already complete */
		if (max_len < orig_len)
		{
			*status = ERR_FILENAMETOOLONG;
			return FALSE;

		}
		length = orig_len;
		memcpy(full_fn, orig_fn, length);
	} else
	{
		GETCWD(cwdbuf, SIZEOF(cwdbuf), getcwd_res);
		if (NULL == getcwd_res)
		{
			*status = errno;
			return FALSE;
		}
		cwd_len = strlen(cwdbuf);
		cptr = orig_fn;
		if (('.' == *cptr)  &&  ('.' == *(cptr + 1)))
		{
			for (i = 1;  ;  ++i)
			{
				cptr += 2;
				if (('.' != *(cptr + 1))  ||  ('.' != *(cptr + 2)))
					break;
				++cptr;
			}
			for (c1 = &cwdbuf[cwd_len - 1];  i > 0;  --i)
				while ('/' != *c1)
					--c1;
			assert(c1 >= cwdbuf);
			dir_len = (unsigned int)(c1 - cwdbuf);
			assert(cptr >= orig_fn);
			trim_len = (unsigned int)(cptr - orig_fn);
			assert(orig_len >= trim_len);
			newfn_len = orig_len - trim_len;
			length = dir_len + newfn_len;
			if (max_len < (length + 1))
			{
				*status = ERR_FILENAMETOOLONG;
				return FALSE;
			}
			memcpy(full_fn, cwdbuf, dir_len);
			memcpy(full_fn + dir_len, cptr, newfn_len);
		} else
		{
			if ('.' == *cptr && '/' == (*(cptr + 1)))
				cptr += 2;
			assert(cptr >= orig_fn);
			trim_len = (unsigned int)(cptr - orig_fn);
			assert(orig_len >= trim_len);
			newfn_len = orig_len - trim_len;
			length = cwd_len + 1 + newfn_len;
			if (max_len < (length + 1))
			{
				*status = ERR_FILENAMETOOLONG;
				return FALSE;
			}
			memcpy(full_fn, cwdbuf, cwd_len);
			full_fn[cwd_len++] = '/';
			memcpy(full_fn + cwd_len, cptr, newfn_len);
		}
	}
	*full_len = length;
	/*Remove multiple slash occurances*/
        *full_len = rmv_mul_slsh(full_fn, *full_len);
	full_fn[*full_len] = '\0';
	return TRUE;
}
