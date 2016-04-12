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

#include "gtm_stdio.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_limits.h"
#include <errno.h>

#include "have_crit.h"
#include "setzdir.h"

UNSUPPORTED_PLATFORM_CHECK

static char	directory_buffer[GTM_MAX_DIR_LEN];

error_def(ERR_SETZDIR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

void setzdir(mval *newdir, mval *full_path_of_newdir)
{	/* newdir is the directory to change to; NULL to set full_path_of_newdir to current working directory.
	 * If full_path_of_newdir is non NULL, return the full path of the new directory in full_path_of_newdir.
	 * NOTE : the full path of directory is stored in a static buffer which might get overwritten by the next call to setzdir.
	 * Callers should save return value if needed.
	 */
	char 		directory[GTM_MAX_DIR_LEN], *getcwd_res, *err_str;
	uint4		length, status;

	assert(NULL != newdir || NULL != full_path_of_newdir);
	if (NULL != newdir)
	{
		MV_FORCE_STR(newdir);
		assert(SIZEOF(directory) > newdir->str.len);
		memcpy(directory, newdir->str.addr, newdir->str.len);
		directory[newdir->str.len] = '\0';
		if (-1 == CHDIR(directory))
		{	/* On VMS, chdir(directory, 0) [actually, any non 1 value] is supposed to restore the process startup cwd at
			 * exit (see help cc run). We've noticed that it doesn't behave the way it has been documented in the mumps
			 * executable. Vinaya, 08/22/2001.
			 */
			err_str = STRERROR(errno);
			rts_error(VARLSTCNT(8) ERR_SETZDIR, 2, newdir->str.len, newdir->str.addr, ERR_TEXT, 2,
					LEN_AND_STR(err_str));
        	}
	}
	/* We need to find the full path of the current working directory because newdir might be a relative path, in which case
	 * $ZDIR will show up as a relative path.
	 */
	if (NULL != full_path_of_newdir)
	{
		GETCWD(directory_buffer, SIZEOF(directory_buffer), getcwd_res);
		if (NULL != getcwd_res)
		{
			length = USTRLEN(directory_buffer);
			UNIX_ONLY(directory_buffer[length++] = '/';)
		} else
		{
			err_str = STRERROR(errno);
			rts_error(VARLSTCNT(11) ERR_SYSCALL, 5, LEN_AND_LIT("getcwd"), CALLFROM, ERR_TEXT, 2, LEN_AND_STR(err_str));
		}
		full_path_of_newdir->mvtype = MV_STR;
		full_path_of_newdir->str.addr = directory_buffer;
		full_path_of_newdir->str.len = length;
	}
	return;
}
