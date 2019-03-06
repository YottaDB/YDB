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

#ifndef DB_READ_H_INCLUDED
#define DB_READ_H_INCLUDED

int4 db_read (int4 fdesc, off_t fptr, sm_uc_ptr_t fbuff, size_t fbuff_len);

#endif
