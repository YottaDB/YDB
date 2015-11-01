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

#ifndef DISK_BLOCK_AVAILABLE_INCLUDED
#define DISK_BLOCK_AVAILABLE_INCLUDED

int4 disk_block_available(int fd, GTM_BAVAIL_TYPE *ret, boolean_t fill_unix_holes);

#endif /* DISK_BLOCK_AVAILABLE_INCLUDED */
