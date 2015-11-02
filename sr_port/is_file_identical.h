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

#ifndef __IS_FILE_IDENTICAL_H__
#define __IS_FILE_IDENTICAL_H__

bool 		is_file_identical(char *filename1, char *filename2);
bool		is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen);
#ifdef VMS
void 		set_gdid_from_file(gd_id_ptr_t fileid, char *filename, int4 filelen);
bool 		is_gdid_gdid_identical(gd_id_ptr_t fid_1, gd_id_ptr_t fid_2);
#elif defined(UNIX)
#include "gtm_stat.h"
bool		is_gdid_identical(gd_id_ptr_t fid1, gd_id_ptr_t fid2);
bool 		is_gdid_stat_identical(gd_id_ptr_t fid, struct stat *stat_buf);
void		set_gdid_from_stat(gd_id_ptr_t fid, struct stat *stat_buf);
boolean_t 	filename_to_id(gd_id_ptr_t fid, char *filename);
#endif
#endif
