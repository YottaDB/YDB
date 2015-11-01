/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MDEFSA_included
#define MDEFSA_included

/* Declarations common to all unix mdefsp.h, to be moved here */

/* DSK_WRITE Macro needs <errno.h> to be included. */
#define	DSK_WRITE(reg, blk, ptr, status)			\
{								\
	if (-1 == dsk_write(reg, blk, ptr))			\
		status = errno;					\
	else							\
		status = 0;					\
}

#define DOTM		".m"
#define DOTOBJ		".o"

#endif /* MDEFSA_included */
