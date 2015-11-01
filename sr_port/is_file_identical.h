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

#ifndef __IS_FILE_IDENTICAL_H__
#define __IS_FILE_IDENTICAL_H__

bool 		is_file_identical(char *filename1, char *filename2);
bool 		is_gdid_gdid_identical(gd_id_ptr_t fid_1, gd_id_ptr_t fid_2);
#ifdef VMS
bool 		is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen);
void 		set_gdid_from_file(gd_id_ptr_t fileid, char *filename, int4 filelen);
#elif defined(UNIX)
#include "gtm_stat.h"
bool 		is_gdid_stat_identical(gd_id_ptr_t fid, struct stat *stat_buf);
void		set_gdid_from_stat(gd_id_ptr_t fid, struct stat *stat_buf);
boolean_t 	filename_to_id(char *filename, char *unique_id);
void 		stat_to_id(struct stat *filestat, char *unique_id);
#else
#error Unsupported Platform
#endif

#endif

