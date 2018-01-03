/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GDS_RUNDOWN_INCLUDED
#define GDS_RUNDOWN_INCLUDED

#define	CLEANUP_UDI_FALSE	FALSE
#define	CLEANUP_UDI_TRUE	TRUE

int4 gds_rundown(boolean_t cleanup_udi);

#define CAN_BYPASS(SEMVAL, CSA, INST_IS_FROZEN)											\
	(INST_IS_FROZEN || FROZEN_CHILLED(CSA)											\
		|| (IS_GTM_IMAGE && (CSA)->hdr->mumps_can_bypass && (PROC_FACTOR * (num_additional_processors + 1) < SEMVAL))	\
		|| (((2 * DB_COUNTER_SEM_INCR) < SEMVAL) && (IS_LKE_IMAGE || IS_DSE_IMAGE)))

#define CANCEL_DB_TIMERS(region, csa, cancelled_dbsync_timer)			\
{										\
	if (csa->timer)								\
	{									\
		cancel_timer((TID)region);					\
		if (NULL != csa->nl)						\
		{								\
			DECR_CNT(&csa->nl->wcs_timers, &csa->nl->wc_var_lock);	\
			REMOVE_WT_PID(csa);					\
		}								\
		csa->canceled_flush_timer = TRUE;				\
		csa->timer = FALSE;						\
	}									\
	if (csa->dbsync_timer)							\
	{									\
		CANCEL_DBSYNC_TIMER(csa);					\
		cancelled_dbsync_timer = TRUE;					\
	}									\
}

/* A multiplicative factor to the # of processors used in determining if a GT.M process in gds_rundown can bypass semaphores */
#ifdef DEBUG
#	define PROC_FACTOR	2
#else
#	define PROC_FACTOR	20
#endif

#endif /* GDS_RUNDOWN_INCLUDED */
