/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef IOSB_DISK_H_INCLUDED
#define IOSB_DISK_H_INCLUDED

#pragma member_alignment save
#pragma nomember_alignment

typedef struct io_status_block_disk_struct
{
	unsigned short	cond;
	unsigned int	length;
	unsigned short	devdepend;
} io_status_block_disk;

#pragma member_alignment restore

#endif /* IOSB_DISK_H_INCLUDED */
