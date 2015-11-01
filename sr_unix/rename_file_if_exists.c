/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include "gtm_stat.h"
#include "gtm_ulimit.h"
#include "gtm_stdio.h"
#include "gtm_time.h"

/* Following 5 is only to get MAX_FN_LEN */
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "eintr_wrappers.h"
#include "rename_file_if_exists.h"
#include "send_msg.h"
#include "gtmmsg.h"

GBLREF bool run_time;

#define TIME_EXT_FMT "_%Y%j%H%M%S"   	/* .yearjuliendayhoursminutesseconds */
#define TIME_EXT_LEN 14			/* 4 digit year, 3 digit joul day, 6 digit time 1 null */
#define MAX_CHARS_APPEND 10

/* --------------------------------------------------------------------------------
	This function  renames a file, if exists. Otherwise do nothing.
	Returns: TRUE, if renames successful
		 TRUE, if rename is not required because it does not exists
		 FALSE, if rename fails
  --------------------------------------------------------------------------------- */
int rename_file_if_exists(char *org_fn, int org_fn_len, int4 *info_status, char *rename_fn, int *rename_len)
{
	struct stat	stat_buf;
	struct tm	*tm_struct;
	int		stat_res, length, cnt;
	uint4		curr_time;
	char		append_char[MAX_CHARS_APPEND] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	error_def(ERR_FILERENAME);

	memcpy(rename_fn, org_fn, org_fn_len + 1); /* Ensure it to be NULL terminated */
	*rename_len = org_fn_len;
	STAT_FILE(org_fn, &stat_buf, stat_res);
	if (-1 != stat_res) /* if file org_fn exists */
	{
		assert(0 <  MAX_FN_LEN - org_fn_len - 1);
		tm_struct = localtime(&(stat_buf.st_ctime));
		STRFTIME(&rename_fn[*rename_len], MAX_FN_LEN - *rename_len - 1, TIME_EXT_FMT, tm_struct, length);
		length += *rename_len;
		STAT_FILE(rename_fn, &stat_buf, stat_res);
		if (-1 != stat_res) /* if exists */
		{
			/* new name refers to an existing file - stuff numbers on the end until its unique */
			rename_fn[length++] = '_';
			for ( ; length < MAX_FN_LEN ; length++)
			{
				rename_fn[length + 1] = '\0';
				for (cnt = 0; cnt < MAX_CHARS_APPEND; cnt++)
				{
					rename_fn[length] = append_char[cnt];
					STAT_FILE(rename_fn, &stat_buf, stat_res);
					if (-1 == stat_res && ENOENT == errno) /* if does not exist */
						break;
				}
				if (cnt < MAX_CHARS_APPEND) /* found one non existance file */
					break;
			}
			length++;
			if (MAX_FN_LEN <= length)
			{
				*info_status = ENAMETOOLONG;
				return RENAME_FAILED;
			}
		}
		if (RENAME(org_fn, rename_fn) == -1)
		{
			*info_status = errno;
			return RENAME_FAILED;
		}
		*rename_len = length;
	}
	else if (ENOENT != errno) /* some error happened */
	{
		*info_status = errno;
		return RENAME_FAILED;
	}
	else
		return RENAME_NOT_REQD;
	/* else (ENOENT == ) no file exits of given name org_fn, so do not rename, just return */
	if (run_time)
		send_msg(VARLSTCNT (6) ERR_FILERENAME, 4, org_fn_len, org_fn, *rename_len, rename_fn);
	else
		gtm_putmsg(VARLSTCNT (6) ERR_FILERENAME, 4, org_fn_len, org_fn, *rename_len, rename_fn);
	return RENAME_SUCCESS;
}
