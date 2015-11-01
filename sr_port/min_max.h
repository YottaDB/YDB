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

#ifndef	MIN_MAX_included
#define MIN_MAX_included

#undef MIN
#undef MAX
#undef ABS

#define MIN(a,b) 	((a) < (b) ? (a) : (b))
#define MAX(a,b) 	((a) > (b) ? (a) : (b))
#define	ABS(a)		((a) > 0 ? (a) : -(a))

#endif
