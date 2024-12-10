/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef IS_GDID_H_INCLUDED
#define IS_GDID_H_INCLUDED

#include "gtm_stat.h"
bool		is_gdid_file_identical(gd_id_ptr_t fid, char *filename, int4 filelen, int *error);
bool		is_gdid_identical(gd_id_ptr_t fid1, gd_id_ptr_t fid2);
bool 		is_gdid_stat_identical(gd_id_ptr_t fid, struct stat *stat_buf);
void		set_gdid_from_stat(gd_id_ptr_t fid, struct stat *stat_buf);
uint4	 	filename_to_id(gd_id_ptr_t fid, char *filename);

#endif
