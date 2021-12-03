/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* is_file_identical.c
 *   returns TRUE   if the two files are identical,
 *   returns FALSE  if 1. either one of the files specified
 *                        doesn't exist, or
 *                     2. they are different files.
 */

#include "mdef.h"

#include "gtm_stdlib.h"

#include "gtm_stat.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "eintr_wrappers.h"
#include "copy.h"
#include "is_file_identical.h"
#include "is_gdid.h"
#include "iosp.h"		/* for SS_NORMAL */

bool is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen)
{
	int		stat_res;
	struct stat	stat_buf;

	assert(0 == filename[filelen]);
	STAT_FILE(filename, &stat_buf, stat_res);
	return is_gdid_stat_identical(fid, &stat_buf);
}
bool is_file_identical(char *filename1, char *filename2)
{
	int		rv = FALSE;
	int		stat_res;
	struct stat	st1, st2;

	STAT_FILE(filename1, &st1, stat_res);
	if (0 == stat_res)
	{
	        STAT_FILE(filename2, &st2, stat_res);
		if (0 == stat_res)
			if ((st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino))
				rv = TRUE;
	}
	return rv;
}

bool  is_gdid_identical(gd_id_ptr_t fid1, gd_id_ptr_t fid2)
{
	bool rv = FALSE;

	if ((fid1->inode == fid2->inode) && (fid1->device == fid2->device))
		rv = TRUE;
	return rv;
}

bool is_gdid_stat_identical(gd_id_ptr_t fid, struct stat *stat_buf)
{
	if (fid->device == stat_buf->st_dev && fid->inode == stat_buf->st_ino)
		return TRUE;
	else
		return FALSE;
}

void set_gdid_from_stat(gd_id_ptr_t fid, struct stat *stat_buf)
{	/* AIX/JFS has data in stat_buf->st_gen that seems related to relative time, but it's garbage for remote (NFS) files
	 * and for files on non-OSF decended implementations, so we conclude it's irrelevant and ignore it;
	 * because we use our own ftok generation there should be no consequence to this approach
	 */
	assert(SIZEOF(gd_id) <= SIZEOF(gds_file_id));
	fid->inode = stat_buf->st_ino;
	fid->device = stat_buf->st_dev;
}

/*
 * Here we create a unique_id for a file.
 */
uint4 filename_to_id(gd_id_ptr_t fid, char *filename)
{
	int		stat_res;
	struct stat	filestat;

	STAT_FILE(filename, &filestat, stat_res);
	if (-1 == stat_res)
		return errno;
	set_gdid_from_stat(fid, &filestat);
	return SS_NORMAL;
}
