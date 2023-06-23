/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "eintr_wrappers.h"

#define	MAX_CHARS_APPEND	10
#define	DIR_FILE_SEPARATOR	'/'

error_def(ERR_FILENAMETOOLONG);

/* Given org_fn this will create rename_fn
 *	a) If prefix is not null, rename_fn = prefix + org_fn
 *	b) If suffix is not null, rename_fn = org_fn + suffix
 *	c) If prefix and suffix are both null, then
 *		rename_fn = org_fn + timestamp
 *		If rename_fn exists, add numbers to make it non-existance file
 *      d) Note: This function cannot handle the case wehre prefix and suffix are both non-null. Callers ensure that.
 * At function entry, "rename_fn_len" points to the allocated length of the "rename_fn" char array.
 * On a successful function exit, "rename_fn_len" is set to the length of "rename_fn".
 */
uint4 	prepare_unique_name(char *org_fn, int org_fn_len, char *prefix, char *suffix,
		char *rename_fn, int *rename_fn_len, jnl_tm_t now, uint4 *ustatus)
{
	mstr 		filestr;
	char		*filename_begin, append_char[MAX_CHARS_APPEND] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
	int		prefix_len, suffix_len, cnt, length;
	struct tm	*tm_struct;
	int		file_stat, rename_alloc_len;
	uint4		status1;
	boolean_t	nonnull_prefix, nonnull_suffix;
	time_t		tt_now;
	size_t          tm_str_len, avail_len;
	char		*tm_str;

	*ustatus = SS_NORMAL;
	rename_alloc_len = *rename_fn_len;	/* Note we need space for null terminator too at end of the renamed file name */
	assert(rename_alloc_len);	/* ensure caller has set this */
	nonnull_prefix = (0 != prefix[0]);
	nonnull_suffix = (0 != suffix[0]);
	assert(!nonnull_prefix || !nonnull_suffix);	/* both prefix & suffix cannot be non-null */
	if (nonnull_prefix)
	{
		prefix_len = STRLEN(prefix);
		assert('\0' == org_fn[org_fn_len]);
		if ((prefix_len + org_fn_len) >= rename_alloc_len)
	 		return ERR_FILENAMETOOLONG;
		filename_begin = strrchr(org_fn, DIR_FILE_SEPARATOR);
		if (NULL != filename_begin)
		{
			length = (int4)(filename_begin - org_fn + 1);
			assert(length < org_fn_len);
			memcpy(rename_fn, org_fn, length);
			org_fn_len -= length;
			org_fn += length;
		} else
			length = 0;
		memcpy(rename_fn + length, prefix, prefix_len);
		memcpy(rename_fn + length + prefix_len, org_fn, org_fn_len);
		*rename_fn_len = length + prefix_len + org_fn_len;
	}
	if (nonnull_suffix)
	{
		suffix_len = STRLEN(suffix);
		if ((org_fn_len + suffix_len) >= rename_alloc_len)
	 		return ERR_FILENAMETOOLONG;
		memcpy(rename_fn, org_fn, org_fn_len);
		memcpy(rename_fn + org_fn_len, suffix, suffix_len);
		*rename_fn_len = org_fn_len + suffix_len;
	}
	if (nonnull_prefix || nonnull_suffix)
	{
		assert(*rename_fn_len < rename_alloc_len);
		rename_fn[*rename_fn_len] = 0; /* Ensure it to be NULL terminated */
		return SS_NORMAL;
	}
	/* Append formatted timestamp to "rename_fn" */
	if (rename_alloc_len <= org_fn_len)
	 	return ERR_FILENAMETOOLONG;
	tm_str = &rename_fn[org_fn_len];
	avail_len = rename_alloc_len - org_fn_len;
	tt_now = (time_t)now;
	GTM_LOCALTIME(tm_struct, &tt_now);
	STRFTIME(tm_str, avail_len, JNLSWITCH_TM_FMT, tm_struct, tm_str_len);
	if (!tm_str_len) /* STRFTIME can return 0 if space is not enough to store entire time string including null terminator */
	 	return ERR_FILENAMETOOLONG;
	assert(JNLSWITCH_TM_FMT_LEN == tm_str_len);
	memcpy(rename_fn, org_fn, org_fn_len); /* Copy original file name to "rename_fn" */
	filestr.addr = rename_fn;
	filestr.len = org_fn_len + (int)tm_str_len;
	assert(rename_alloc_len > filestr.len);
	assert('\0' == rename_fn[filestr.len]);
	if (FILE_PRESENT == (file_stat = gtm_file_stat(&filestr, NULL, NULL, FALSE, ustatus))) /* One already exists */
	{	/* new name refers to an existing file - stuff numbers on the end until its unique */
		assert(rename_alloc_len > filestr.len);
		rename_fn[filestr.len] = '_';	/* this access is safe from buffer overflow because of the above assert */
		filestr.len += 2; /* 2 is for "_" + "append_char[]" */
		for ( ; ; filestr.len++)
		{
			if (rename_alloc_len <= filestr.len)
				return ERR_FILENAMETOOLONG;
			rename_fn[filestr.len] = '\0';
			for (cnt = 0; MAX_CHARS_APPEND > cnt; cnt++)
			{
				rename_fn[filestr.len - 1] = append_char[cnt];
				if (FILE_NOT_FOUND == gtm_file_stat(&filestr, NULL, NULL, FALSE, ustatus))
					break;
			}
			if (MAX_CHARS_APPEND > cnt) /* found one non-existing file */
				break;
		}
		*ustatus = SS_NORMAL;
	} else if (FILE_STAT_ERROR == file_stat)
	{
		status1 = *ustatus;
		assert(SS_NORMAL != status1);
		*ustatus = SS_NORMAL;
	 	return status1;
	}
	assert(rename_alloc_len > filestr.len);
	*rename_fn_len = filestr.len;
	assert('\0' == rename_fn[filestr.len]);
	return SS_NORMAL;
}
