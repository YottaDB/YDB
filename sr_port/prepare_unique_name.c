/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"

#include "iosp.h"
#include "gtm_file_stat.h"
#include "gtm_rename.h"
#include "gtmmsg.h"

#define MAX_CHARS_APPEND 10

#if defined(UNIX)
# define	DIR_FILE_SEPARATOR	'/'
#elif defined(VMS)
# define	DIR_FILE_SEPARATOR	']'
#endif

/* Given org_fn this will create rename_fn
 *	a) If prefix is not null, rename_fn = prefix + org_fn
 *	b) If suffix is not null, rename_fn = org_fn + suffix
 *	c) If prefix and suffix are both null, then
 *		rename_fn = org_fn + timestamp
 *		If rename_fn exists, add numbers to make it non-existance file
 */
uint4 	prepare_unique_name(char *org_fn, int org_fn_len, char *prefix, char *suffix,
		char *rename_fn, int *rename_fn_len, jnl_tm_t now, uint4 *ustatus)
{
	mstr 		filestr;
	char		*filename_begin, append_char[MAX_CHARS_APPEND] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	int		prefix_len, suffix_len, cnt, length;
	struct tm	*tm_struct;
	int		file_stat;
	uint4		status1;

	error_def(ERR_FILENAMETOOLONG);

	*ustatus = SS_NORMAL;
	if (0 != prefix[0])
	{
		prefix_len = STRLEN(prefix);
		assert('\0' == org_fn[org_fn_len]);
		filename_begin = strrchr(org_fn, DIR_FILE_SEPARATOR);
		length = 0;
		if (NULL != filename_begin)
		{
			length = (int4)(filename_begin - org_fn + 1);
			assert(length < org_fn_len);
			memcpy(rename_fn, org_fn, length);
			org_fn_len -= length;
			org_fn += length;
		}
		memcpy(rename_fn + length, prefix, prefix_len);
		memcpy(rename_fn + length + prefix_len, org_fn, org_fn_len);
		*rename_fn_len = length + prefix_len + org_fn_len;
	}
	if (0 != suffix[0])
	{
		suffix_len = STRLEN(suffix);
		memcpy(rename_fn, org_fn, org_fn_len);
		memcpy(rename_fn + org_fn_len, suffix, suffix_len);
		*rename_fn_len = org_fn_len + suffix_len;
	}
	if (0 != prefix[0] || 0 != suffix[0])
	{
		rename_fn[*rename_fn_len] = 0; /* Ensure it to be NULL terminated */
		return SS_NORMAL;
	}
	memcpy(rename_fn, org_fn, org_fn_len);
	filestr.addr = rename_fn;
	filestr.len = org_fn_len;
	rename_fn[filestr.len] = 0;
	assert(filestr.len + 1 < MAX_FN_LEN);
	if (SS_NORMAL != (status1 = append_time_stamp(rename_fn, &filestr.len, now)))
		return status1;
	if (FILE_PRESENT == (file_stat = gtm_file_stat(&filestr, NULL, NULL, FALSE, ustatus))) /* One already exists */
	{	/* new name refers to an existing file - stuff numbers on the end until its unique */
		rename_fn[filestr.len] = '_';
		filestr.len += 2; /* 2 is for "_" + "append_char[]" */
		for ( ; filestr.len < MAX_FN_LEN; filestr.len++)
		{
			rename_fn[filestr.len] = '\0';
			filestr.len = filestr.len;
			for (cnt = 0; cnt < MAX_CHARS_APPEND; cnt++)
			{
				rename_fn[filestr.len - 1] = append_char[cnt];
				if (FILE_NOT_FOUND == gtm_file_stat(&filestr, NULL, NULL, FALSE, ustatus))
					break;
			}
			if (cnt < MAX_CHARS_APPEND) /* found one non existance file */
				break;
		}
		*ustatus = SS_NORMAL;
		if (MAX_FN_LEN <= filestr.len)
	 		return ERR_FILENAMETOOLONG;	/* no parameter for this message */
	} else if (FILE_STAT_ERROR == file_stat)
	{
		status1 = *ustatus;
		*ustatus = SS_NORMAL;
	 	return status1;
	}
	*rename_fn_len = filestr.len;
	return SS_NORMAL;
}
