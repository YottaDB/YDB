/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

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
boolean_t get_full_path(char *orig_fn, unsigned int orig_len, char *full_fn, unsigned int *full_len, int max_len, uint4 *status)
{
	char	*cptr, *c1;
	char	cwdbuf[GTM_PATH_MAX];
	int	cwd_len;
	int	i, length;
	char	*getcwd_res;

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
		cwd_len = STRLEN(cwdbuf);
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
			if ((length = (int)((c1 - cwdbuf) + orig_len - (cptr - orig_fn))) + 1 > max_len) /* Warning - assignment */
			{
				*status = ERR_FILENAMETOOLONG;
				return FALSE;
			}
			memcpy(full_fn, cwdbuf, c1 - cwdbuf);
			memcpy(full_fn + (c1 - cwdbuf), cptr, orig_len - (cptr - orig_fn));
		} else
		{
			if ('.' == *cptr && '/' == (*(cptr + 1)))
				cptr += 2;
			if ((length = (int)(cwd_len + 1 + orig_len - (cptr - orig_fn))) + 1 > max_len)	/* Warning - assignment */
			{
				*status = ERR_FILENAMETOOLONG;
				return FALSE;
			}
			strcpy(full_fn, cwdbuf);
			full_fn[cwd_len] = '/';
			memcpy(full_fn + cwd_len + 1, cptr, orig_len - (cptr - orig_fn));
		}
	}
	*full_len = length;
	full_fn[length] = '\0';
	return TRUE;
}
