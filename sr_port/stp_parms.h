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

/*
 *	STP_PARMS.H - String pool parameters
 */

#define STP_PAGE_SIZE	8192 /* if necessary, change this under appropriate ifdefs for different platforms, or, use macros from
			      * mdefsp.h */
#define STP_INITSIZE	(12 * STP_PAGE_SIZE) /* initial size of string pool */
#define STP_MAXSIZE	(400 * STP_PAGE_SIZE) /* maximum size of string pool */
#define STP_INCREMENT	(4 * STP_PAGE_SIZE) /* amount to grow string pool, also used as minimum stringpool size */
#define STP_MINFREE	(4 * STP_PAGE_SIZE) /* minimum amount which will cause a 'grow' next time */
#define STP_MAXITEMS	8192	/* maximum number of mval's for garbage collection */
#define STP_MAXGEOMGROWTH (16 * 1024 * 1024) /* let the stringpool grow geometrically till the size reaches STP_MAXGEOMGROWTH, after
					      * which, grow linearly */
#define STP_RECLAIMLIMIT (2 * 1024 * 1024) /* if the space reclaimed is more than this amount, don't do forced expansion */
#define STP_LIMITFRCDEXPN  STP_MAXGEOMGROWTH /* no forced expansions of any kind after the stringpool size grows to this limit */
#define STP_GEOMGROWTH_FACTOR	2
#define STP_MINRECLAIM	((stringpool.top - stringpool.base) >> 2) /* atleast 25% of the current size */
#define STP_ENOUGHRECLAIMED ((stringpool.top - stringpool.base) - ((stringpool.top - stringpool.base) >> 3))/* 87.5% of size */
#define STP_MAXLOWRECLAIM_PASSES 4 /* if did not reclaim STP_MINRECLAIM bytes for STP_MAXLOWRECLAIM_PASSES, force an expansion */
#define STP_INITMAXNOEXP_PASSES	4 /* initial value of number of consecutive garbage collection passes that do not expand the
				   * stringpool, after which an expansion is forced */
#define STP_NOEXP_PASSES_INCR	1 /* increase the "number of consecutive gcol passes for a forced expansion" by this amount for
				   * every forced expansion */
#define STP_MAXNOEXP_PASSES	10 /* no forced expansions after "number of consecutive gcol passes for a forced expansion"
				    * reaches this limit. It takes 10 expansions for the strinpool to grow from STP_INITISIZE to
				    * STP_MAXGEOMGROWTH */
