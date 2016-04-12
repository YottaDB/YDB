/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "errno.h"
#include "eintr_wrappers.h"

#include "gtm_string.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "parse_file.h"
#include "gtm_file_stat.h"
#include "gtmmsg.h"

#define DOT '.'

/* Checks the status of a file.
 * Output Parameter
 *	uint4 *status : error number
 * Returns:
 *	FILE_NOT_FOUND: if file is not presnt
 *	FILE_PRESENT:	if file is present
 *	FILE_READONLY|FILE_PRESENT:	if file is readonly
 *	FILE_STAT_ERROR: if error happens during this module.
 *	Side Effect:    Except for FILE_STAT_ERROR passed "ret" will have file name with full path.
 */
int gtm_file_stat(mstr *file, mstr *def, mstr *ret, boolean_t check_prv, uint4 *status)
{
	int 		stat_res;
        struct stat	stat_buf;
	char		fbuff[MAX_FBUFF + 1];
	parse_blk	pblk;
	mstr		tmp_str, *tmpfile;
	boolean_t	file_not_found = FALSE;

	tmpfile = &tmp_str;
	if (NULL != def)
	{
		memset(&pblk, 0, SIZEOF(pblk));
		pblk.buffer = fbuff;
		pblk.buff_size = MAX_FBUFF;
		pblk.fop = (F_SYNTAXO | F_PARNODE);
		memcpy(fbuff, file->addr, file->len);
		*(fbuff + file->len) = '\0';
		if (NULL != def)
		{
			pblk.def1_buf = def->addr;
			pblk.def1_size = def->len;
		}
		*status = parse_file(file, &pblk);
		if (!(*status & 1))
			file_not_found = TRUE;
		pblk.buffer[pblk.b_esl] = 0;
		tmpfile->addr =  pblk.buffer;
		tmpfile->len = pblk.b_esl;
	} else
	{
		tmpfile = file;
		tmpfile->addr[tmpfile->len] = 0;
	}
	if (!file_not_found)
	{
		STAT_FILE((char *)tmpfile->addr, &stat_buf, stat_res);
		if (0 != stat_res)
		{
			*status = errno;
			if (ENOENT == errno)
				file_not_found = TRUE;
			else
				return FILE_STAT_ERROR;
		}
	}
	if (NULL != ret)
		/* We asume ret->addr has MAX_FN_LEN bytes allocated */
		if (!get_full_path((char *)tmpfile->addr,  tmpfile->len, ret->addr, (unsigned int *) &ret->len, MAX_FN_LEN, status))
			 return FILE_STAT_ERROR;
	if (file_not_found)
		return FILE_NOT_FOUND;
	/* Now we know file is present */
	if (check_prv && 0 != ACCESS((char *)tmpfile->addr, W_OK))
		return (FILE_PRESENT | FILE_READONLY);
	else
		return FILE_PRESENT;
}
