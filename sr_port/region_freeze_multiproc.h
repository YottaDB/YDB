/****************************************************************
 *								*
 * Copyright (c) 2020 Fidelity National Information		*
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

typedef struct freeze_multiproc_state_struct
{
	int		ntasks;			/* Number of processes */
	int		grab_crit_counter;	/* Increment counter to grab crit in a specific order */
	int		region_frozen_counter;	/* Increment counter to verify all processes release crit at the same point */
	global_latch_t	region_frozen_latch;	/* Latch to do increment/decrement operations (unused in most platforms) */
	int		freeze_ret_array[];	/* Stash the return values of region_freeze_main() */
} freeze_multiproc_state;

typedef struct freeze_reg_mp_state_struct
{
	int			region_index;	/* Per-region index value, aligned to the order the region appears in grlist. */
	freeze_multiproc_state	*pfms;		/* Pointer to parallel_shm_hdr shared memory */
} freeze_reg_mp_state;

#endif
