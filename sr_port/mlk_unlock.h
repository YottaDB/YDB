/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MLK_UNLOCK_INCLUDED
#define MLK_UNLOCK_INCLUDED

#define LOCK_SPACE_FULL_SYSLOG_THRESHOLD	0.25	/* Minimum free space percentage to write LOCKSPACEFULL to syslog again.
							 * MIN(free_prcblk_ratio, free_shr_blk_ratio) must be greater than this
							 * value to print syslog again (see also: gdsbt.h,
							 * lockspacefull_logged definition
							 */

void mlk_unlock(mlk_pvtblk *p);

#endif /* MLK_UNLOCK_INCLUDED */
