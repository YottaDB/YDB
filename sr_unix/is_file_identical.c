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

bool is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen)
{
        struct stat	stat_buf;
	int		stat_res;

	assert(0 == filename[filelen]);
	STAT_FILE(filename, &stat_buf, stat_res);
	return is_gdid_stat_identical(fid, &stat_buf);
}
bool is_file_identical(char *filename1, char *filename2)
{
        struct stat	st1, st2;
	int		stat_res;
	int		rv = FALSE;

	STAT_FILE(filename1, &st1, stat_res);
	if (0 == stat_res)
	{
	        STAT_FILE(filename2, &st2, stat_res);
	        if (0 == stat_res)
#if defined(__osf__) || defined(_AIX)
		        if ((st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino) && (
#ifdef _AIX
				 (FS_REMOTE == st1.st_flag || FS_REMOTE == st2.st_flag) ? TRUE :
#endif
				    st1.st_gen == st2.st_gen))
#else
		        if ((st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino))
#endif
			        rv = TRUE;
	}
	return rv;
}

bool  is_gdid_identical(gd_id_ptr_t fid1, gd_id_ptr_t fid2)
{
	bool rv = FALSE;
#if defined(__osf__) || defined(_AIX)
	if ((fid1->inode == fid2->inode) && (fid1->device == fid2->device) && (fid1->st_gen == fid2->st_gen))
		rv = TRUE;
#else
	if ((fid1->inode == fid2->inode) && (fid1->device == fid2->device))
		rv = TRUE;
#endif
	return rv;
}


bool is_gdid_stat_identical(gd_id_ptr_t fid, struct stat *stat_buf)
{
#if defined(__osf__) || defined(_AIX)

	assert(SIZEOF(fid->st_gen) >= SIZEOF(stat_buf->st_gen));
	if (fid->device == stat_buf->st_dev && fid->inode == stat_buf->st_ino &&  (
#ifdef _AIX
	     FS_REMOTE == stat_buf->st_flag ? TRUE :
#endif
	     fid->st_gen == stat_buf->st_gen))
#else
	if (fid->device == stat_buf->st_dev && fid->inode == stat_buf->st_ino)
#endif
		return TRUE;
	else
		return FALSE;
}

void set_gdid_from_stat(gd_id_ptr_t fid, struct stat *stat_buf)
{
	assert(SIZEOF(gd_id) <= SIZEOF(gds_file_id));
	fid->inode = stat_buf->st_ino;
	fid->device = stat_buf->st_dev;
#if defined(__osf__)
	fid->st_gen = stat_buf->st_gen;
#elif defined(_AIX)
	if (FS_REMOTE != stat_buf->st_flag)
		fid->st_gen = stat_buf->st_gen;
	else
		fid->st_gen = 0;   /* AIX has garbage for NFS files */
#endif
}

/*
 * Here we create a unique_id for a file.
 */
boolean_t filename_to_id(gd_id_ptr_t fid, char *filename)
{
        struct stat	filestat;
	int		stat_res;

	STAT_FILE(filename, &filestat, stat_res);
	if (stat_res)
		return FALSE;
	set_gdid_from_stat(fid, &filestat);
	return TRUE;
}
