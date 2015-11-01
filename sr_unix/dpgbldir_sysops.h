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

#ifndef __DPGBLDDIR_SYSOPS_H__
#define __DPGBLDDIR_SYSOPS_H__

#include "gtmio.h"

bool comp_gd_addr(gd_addr *gd_ptr, file_pointer *file_ptr);
void fill_gd_addr_id(gd_addr *gd_ptr, file_pointer *file_ptr);
void close_gd_file(file_pointer *file_ptr);
void file_read(file_pointer *file_ptr, int4 size, uchar_ptr_t buff, int4 pos);
void dpzgbini(void);

#endif

