/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stat.h"
#include "eintr_wrappers.h"
#include "is_raw_dev.h"

#define RAW_PATH "/dev/"

bool is_raw_dev(char *path)
{	char *c;
	int stat_res;
	struct stat fileinfo;

	STAT_FILE(path, &fileinfo, stat_res);
	if ((0 == stat_res) &&
	    (S_ISCHR(fileinfo.st_mode) || S_ISBLK(fileinfo.st_mode)))
		return TRUE;
	else
	    	return FALSE;
}
