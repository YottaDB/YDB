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

#ifndef GTM_SEM_INCLUDED
#define GTM_SEM_INCLUDED

union   semun {
	int     val;
	struct  semid_ds *buf;
	u_short *array;
} arg;

#endif /* GTM_SEM_INCLUDED */
