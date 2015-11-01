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

#ifndef MLK_LOCK_INCLUDED
#define MLK_LOCK_INCLUDED

uint4 mlk_lock(mlk_pvtblk *p, uint4 auxown, bool new);

#endif /* MLK_LOCK_INCLUDED */
