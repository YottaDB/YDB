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

#ifndef LOCKHIST_H
#define LOCKHIST_H

#define LOCKHISTS_KEPT	500
#define OP_LOCK_SIZE	4

/* Structure to hold lock history */
typedef struct
{
	sm_int_ptr_t	lock_addr;		/* Address of actual lock */
	int4		lock_pid;		/* Process id of (un)locker */
	char		lock_op[OP_LOCK_SIZE];	/* Operation performed (either OBTN or RLSE) */
} lockhist;

/* Define pointer types for above structure that may be in shared memory and need 64
   bit pointers. */
#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(save)
#  pragma pointer_size(long)
# else
#  error UNSUPPORTED PLATFORM
# endif
#endif

typedef lockhist *lockhist_ptr_t;

#ifdef DB64
# ifdef __osf__
#  pragma pointer_size(restore)
# endif
#endif

#endif
