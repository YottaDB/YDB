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

#ifndef __DB_READ_H__
#define __DB_READ_H__

int4 db_read (int4 fdesc, off_t fptr, sm_uc_ptr_t fbuff, size_t fbuff_len);

#endif
