/****************************************************************
 *								*
 * Copyright (c) 2020-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef REGION_FREEZE_MULTIPROC_INCLUDED
#define REGION_FREEZE_MULTIPROC_INCLUDED

error_def(ERR_SYSCALL);

#define INCR_REG_FROZEN_COUNT(PFMS, WAIT_PROC)							\
MBSTART {											\
	int rval;										\
												\
	pthread_mutex_lock(&(PFMS)->reg_frozen_mutex);						\
	(PFMS)->reg_frozen_counter++;								\
	if ((PFMS)->reg_frozen_counter >= (PFMS)->ntasks)					\
	{	/* Signal all other processes to stop waiting when this is the last process */	\
		PTHREAD_COND_BROADCAST(&(PFMS)->reg_frozen_cond, rval);				\
	}											\
	else if (WAIT_PROC)									\
	{	/* Wait other processes before releasing crit */				\
		do										\
		{	/* A waiting process could be awakened spuriously */			\
			rval = pthread_cond_wait(&(PFMS)->reg_frozen_cond, &(PFMS)->reg_frozen_mutex);	\
			if (0 != rval)								\
			{									\
				send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5,		\
						LEN_AND_LIT("pthread_cond_wait"), CALLFROM, rval, 0);	\
				break;								\
			}									\
		} while	((PFMS)->ntasks > (PFMS)->reg_frozen_counter);				\
	}											\
	pthread_mutex_unlock(&(PFMS)->reg_frozen_mutex);					\
} MBEND

typedef struct freeze_multiproc_state_struct
{
	int			ntasks;			/* Number of processes */
	int			grab_crit_counter;	/* Increment counter to grab crit in a specific order */
	int			reg_frozen_counter;	/* Increment counter to verify all processes release crit
							 * at the same point.
							 */
	global_latch_t		grab_crit_latch;	/* Latch to do increment/decrement operations (unused in most platforms) */
	pthread_mutexattr_t	reg_frozen_mutex_attr;	/* Place it in shared memory makes the cleanup easier */
	pthread_condattr_t	reg_frozen_cond_attr;	/* Place it in shared memory makes the cleanup easier */
	pthread_mutex_t		reg_frozen_mutex;	/* Condition variable mutex */
	pthread_cond_t		reg_frozen_cond;	/* Force processes to wait at the same point with a condition variable */
	int			freeze_ret_array[];	/* Stash the return values of region_freeze_main() */
} freeze_multiproc_state;

typedef struct freeze_reg_mp_state_struct
{
	int			region_index;	/* Per-region index value, aligned to the order the region appears in grlist. */
	freeze_multiproc_state	*pfms;		/* Pointer to parallel_shm_hdr shared memory */
} freeze_reg_mp_state;

#endif
