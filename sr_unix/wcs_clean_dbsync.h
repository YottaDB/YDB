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

#ifndef __WCS_CLEAN_DBSYNC_H__
#define __WCS_CLEAN_DBSYNC_H__

void wcs_clean_dbsync(TID tid, int4 hd_len, sgmnt_addrs **csaptr);

#define	START_DBSYNC_TIMER(CSA, TIM_DEFER_DBSYNC)										\
{																\
	GBLREF	volatile int4		fast_lock_count;									\
																\
	/* Do not allow wcs_wtstart interrupts for the same region in this two-line window (C9J06-003139).			\
	 * or else we could end up in a situation where the timer is started but dbsync_timer is set to FALSE.			\
	 */															\
	assert(0 <= fast_lock_count);												\
	++fast_lock_count;													\
	CSA->dbsync_timer = TRUE;												\
	/* start_timer copies over the data addressed by &csa ensuring wcs_clean_dbsync gets a valid pointer */			\
	start_timer((TID)CSA, TIM_DEFER_DBSYNC, &wcs_clean_dbsync, SIZEOF(CSA), (char *)&CSA);					\
	assert(CSA->dbsync_timer); /* assert that dbsync_timer did not get reset by an interrupt in the previous line */	\
	--fast_lock_count;													\
	assert(0 <= fast_lock_count);												\
}

/* This macro requires gt_timer.h to be #included for the "find_timer" call in the assert below */
#define	CANCEL_DBSYNC_TIMER(CSA, EXPECT_NULL_TID)							\
{													\
	DEBUG_ONLY(											\
		GT_TIMER		*tprev;								\
		GT_TIMER		*tcur;								\
													\
		GBLREF	int		process_exiting;						\
													\
		assert(CSA->dbsync_timer);								\
		tcur = find_timer_intr_safe((TID)CSA, &tprev);						\
		/* In case process is exiting, all timers would have been				\
		 * removed (by exit handler) so we skip the check for timer				\
		 * presence in that case.								\
		 */											\
		assert(process_exiting || (!EXPECT_NULL_TID && (NULL != tcur))				\
			|| (EXPECT_NULL_TID && (NULL == tcur)));					\
	)												\
	cancel_timer((TID)CSA);										\
	CSA->dbsync_timer = FALSE;									\
}
#endif
