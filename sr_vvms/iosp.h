/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#define TRL_OFF 4

#define SS_NORMAL  1
#define SS_NOLOGNAM  444
#define SS_ENDOFFILE 2160
#define SS_ENDOFTAPE 2168

/* parameters for io_rundown() */
#define NORMAL_RUNDOWN		0
#define RUNDOWN_EXCEPT_STD	1

#define SYSCALL_SUCCESS(STATUS)		(1 & (STATUS))
#define SYSCALL_ERROR(STATUS)		(!(1 & (STATUS)))
