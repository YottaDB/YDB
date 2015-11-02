/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *	STP_PARMS.H - String pool parameters
 */

#define INITIAL_STP_PAGES 12 /* This is in 8K pages */
#define STP_PAGE_SIZE	8192 /* if necessary, change this under appropriate ifdefs for different platforms, or, use macros from
			 * mdefsp.h */
#define STP_INITSIZE	(12 * STP_PAGE_SIZE) /* default initial size of string pool */
#define STP_MAXINITSIZE	(61035 * STP_PAGE_SIZE) /* maximum initial size of string pool */
#define STP_INITSIZE_REQUESTED  (INITIAL_STP_PAGES * STP_PAGE_SIZE) /* requested size of string pool */
#define STP_MAXITEMS	8192	/* initial number of mval's for garbage collection; also grow by this value*/
#define STP_NUM_INCRS	4 /* number of increments on the sliding scale used to grow string pool. Should be a power of 2 so that
			 * so that divides can be done as shifts */
#define STP_LOWRECLAIM_LEVEL(x) ((x >> 2) + (x >> 3) - (x >> 4)) /* level of available string pool (after reclaim) to
			 * count as a low reclaim pass (31.25%) */
#define STP_MAXLOWRECLAIM_PASSES 4 /* after compaction, if at least STP_LOWRECLAIM_LEVEL of string pool is not free for
			 * STP_MAXLOWRECLAIM_PASSES, force an expansion */
