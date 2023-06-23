/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DPGBLDDIR_SYSOPS_H_INCLUDED
#define DPGBLDDIR_SYSOPS_H_INCLUDED

#include "gtmio.h"

bool comp_gd_addr(gd_addr *gd_ptr, file_pointer *file_ptr);
void fill_gd_addr_id(gd_addr *gd_ptr, file_pointer *file_ptr);
void close_gd_file(file_pointer *file_ptr);
void file_read(file_pointer *file_ptr, int4 size, uchar_ptr_t buff, int4 pos);
void dpzgbini(void);

#endif

