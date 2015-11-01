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

#ifndef MUPIP_REORG_DEFINED

/* rename_file_if_exists.h */
#define RENAME_SUCCESS 0
#define RENAME_NOT_REQD 1
#define RENAME_FAILED 2
#define MUPIP_REORG_DEFINED
#endif
int rename_file_if_exists(char *org_fn, int org_fn_len, int4 *info_status, char *rename_fn, int *rename_len);
