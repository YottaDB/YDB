/****************************************************************
 *								*
 *	Copyright 2011, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "mprof.h"

#define MPROF_CHUNK_SIZE 8096	/* previously, allocation for debug was smaller; however, since we have not seen any issues,
				 * we made it equal with pro */
#define MPROF_STACK_ALLOC_CNT (MPROF_CHUNK_SIZE / SIZEOF(mprof_stack_frame))	/* size of allocation chunk in number of frames */

GBLREF int process_exiting;

/* Preallocate space for MPROF_STACK_ALLOC_CNT elements of the stack. */
void mprof_stack_init(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (NULL == TREF(mprof_stack_next_frame))
		TREF(mprof_stack_next_frame) = (mprof_stack_frame *)malloc(SIZEOF(mprof_stack_frame) * MPROF_STACK_ALLOC_CNT);
	TREF(mprof_stack_curr_frame) = NULL;
	TREF(mprof_chunk_avail_size) = MPROF_STACK_ALLOC_CNT;
	return;
}

/* Push a new frame onto the MPROF stack and return a reference to it. */
mprof_stack_frame *mprof_stack_push(void)
{
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (0 == TREF(mprof_chunk_avail_size))
	{
		TREF(mprof_stack_next_frame) = (mprof_stack_frame *)malloc(SIZEOF(mprof_stack_frame) * MPROF_STACK_ALLOC_CNT);
		TREF(mprof_chunk_avail_size) = MPROF_STACK_ALLOC_CNT;
	}
	(TREF(mprof_stack_next_frame))->prev = TREF(mprof_stack_curr_frame);
	TREF(mprof_stack_curr_frame) = TREF(mprof_stack_next_frame);
	(TREF(mprof_stack_next_frame))++;
	(TREF(mprof_chunk_avail_size))--;
	return TREF(mprof_stack_curr_frame);
}

/* Pop the top frame off the MPROF stack. */
mprof_stack_frame *mprof_stack_pop(void)
{
	mprof_stack_frame *last_frame;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	last_frame = (TREF(mprof_stack_curr_frame))->prev;
	if (NULL == TREF(mprof_stack_curr_frame))
		assert(FALSE);
	/* only let mprof_chunk_avail_size become 0 if there have been previously allocated chunks */
	if (((MPROF_STACK_ALLOC_CNT - 1) == TREF(mprof_chunk_avail_size)) && (NULL != last_frame))
	{
		free(TREF(mprof_stack_curr_frame));
		TREF(mprof_chunk_avail_size) = -1;
	}
	TREF(mprof_stack_next_frame) = TREF(mprof_stack_curr_frame);
	TREF(mprof_stack_curr_frame) = last_frame;
	(TREF(mprof_chunk_avail_size))++;
	return TREF(mprof_stack_curr_frame);
}

/* Free the memory allocated for MPROF stack. */
void mprof_stack_free(void)
{
	mprof_stack_frame *chunk_start, *prev_chunk_start;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (process_exiting)	/* no point trying to clean after ourselves if we are exiting */
		return;
	/* there are no elements on the stack */
	if (MPROF_STACK_ALLOC_CNT == TREF(mprof_chunk_avail_size))
	{
		free(TREF(mprof_stack_next_frame));
		TREF(mprof_stack_next_frame) = TREF(mprof_stack_curr_frame) = NULL;
		TREF(mprof_chunk_avail_size) = 0;
		return;
	}
	chunk_start = TREF(mprof_stack_curr_frame) - (MPROF_STACK_ALLOC_CNT - TREF(mprof_chunk_avail_size) - 1);
	while (NULL != chunk_start)
	{
		if (NULL != chunk_start->prev)
			prev_chunk_start = chunk_start->prev - (MPROF_STACK_ALLOC_CNT - 1);
		else
			prev_chunk_start = NULL;
		free(chunk_start);
		chunk_start = prev_chunk_start;
	}
	TREF(mprof_stack_next_frame) = TREF(mprof_stack_curr_frame) = NULL;
	TREF(mprof_chunk_avail_size) = 0;
	return;
}
