/****************************************************************
 *								*
 * Copyright (c) 2016-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <gtm_stdio.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "memcoherency.h"
#include "interlock.h"
#include "is_proc_alive.h"

GBLREF	uint4		process_id;

#define	PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt, currWriteaddr)	\
MBSTART {												\
	DEBUG_ONLY(qw_off_t	start_write_addr;)							\
													\
	assert(phs2cmt < maxCmt);									\
	do												\
	{												\
		if (!phs2cmt->write_complete)								\
			break;										\
		DEBUG_ONLY(currWriteaddr += phs2cmt->tot_jrec_len;)					\
		phs2cmt++;										\
		if (phs2cmt == maxCmt)									\
			break;										\
		DEBUG_ONLY(start_write_addr = phs2cmt->start_write_addr;)				\
		assert(currWriteaddr == start_write_addr);						\
	} while (TRUE);											\
} MBEND

/* The below code is very similar to "jnl_phase2_cleanup" */
void	repl_phase2_cleanup(jnlpool_addrs *jpa)
{
	sgmnt_addrs		*csa;
	qw_off_t		currWriteaddr, newWriteaddr;
	uint4			stuckPid;
	int			index1, index2;
	jpl_phase2_in_prog_t	*phs2cmt, *deadCmt, *begCmt, *maxCmt, *topCmt;
	jnlpool_ctl_ptr_t	jpl;
	boolean_t		was_latch_owner;

	assert(jpa && jpa->jnlpool_dummy_reg);
	csa = &FILE_INFO(jpa->jnlpool_dummy_reg)->s_addrs;
	jpl = jpa->jnlpool_ctl;
	assert(jpl);
	/* It is possible we already own the latch in case we are in timer-interrupt or process-exit code hence the below check */
	was_latch_owner = GLOBAL_LATCH_HELD_BY_US(&jpl->phase2_commit_latch);
	if (!was_latch_owner)
	{	/* Return value of "grab_latch" does not need to be checked because we pass GRAB_LATCH_INDEFINITE_WAIT as timeout */
		grab_latch(&jpl->phase2_commit_latch, GRAB_LATCH_INDEFINITE_WAIT);
	}
	index1 = jpl->phase2_commit_index1;
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index1, JPL_PHASE2_COMMIT_ARRAY_SIZE);
	index2 = jpl->phase2_commit_index2;
	SHM_READ_MEMORY_BARRIER;	/* see corresponding SHM_WRITE_MEMORY_BARRIER in UPDATE_JPL_RSRV_WRITE_ADDR macro */
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index2, JPL_PHASE2_COMMIT_ARRAY_SIZE);
	phs2cmt = &jpl->phase2_commit_array[index1];
	if (index1 != index2)
	{
		if (!phs2cmt->write_complete && csa->now_crit)
		{	/* "repl_phase2_write" not complete in earliest phase2 entry. Since that is blocking us in crit,
			 * check if the pid is alive. If not, cleanup. If you find the first blocking pid is dead, check
			 * the next few pids too until an alive pid or phase2-complete entry shows up.
			 */
			deadCmt = phs2cmt;
			assert(deadCmt->start_write_addr == jpl->write_addr);
			if (index1 < index2)
				maxCmt = &jpl->phase2_commit_array[index2];
			else
				maxCmt = &jpl->phase2_commit_array[JPL_PHASE2_COMMIT_ARRAY_SIZE];
			assert(deadCmt < maxCmt);
			do
			{
				if (deadCmt->write_complete)
					break;
				stuckPid = deadCmt->process_id;
				/* Note that we can get stuck while inside "jnl_pool_write" in phase2 of commit
				 * waiting for phase2 commit gaps in jnlpool to close enough to let us proceed with
				 * our phase2 commit. In that case we can find our pid entry here. If so "break".
				 */
				if (stuckPid == process_id)
					break;
				JPL_TRACE_PRO(jpl, repl_phase2_cleanup_isprcalv);
				if (is_proc_alive(stuckPid, 0))
					break;
				repl_phase2_salvage(jpa, jpl, deadCmt);
				assert(deadCmt->write_complete);
			} while (++deadCmt < maxCmt);
			/* If "(deadCmt == maxCmt) && (index < index2)", we do not wrap around and search for dead pids in
			 * the beginning of the array. Instead the next call to "repl_phase2_cleanup" will take care of
			 * cleaning that section IF there are any deadCmt entries.
			 */
		}
		if (phs2cmt->write_complete)
		{	/* There is at least one phase2 that is complete and can be cleaned up */
			DEBUG_ONLY(currWriteaddr = jpl->write_addr);
			assert(currWriteaddr == phs2cmt->start_write_addr);
			begCmt = &jpl->phase2_commit_array[0];
			if (index1 < index2)
			{	/* Easier case. No wrapping needed. */
				maxCmt = &jpl->phase2_commit_array[index2];
				assert(begCmt < maxCmt);
				PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt, currWriteaddr);
				index1 = phs2cmt - begCmt;
			} else
			{
				maxCmt = topCmt = &jpl->phase2_commit_array[JPL_PHASE2_COMMIT_ARRAY_SIZE];
				PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt, currWriteaddr);
				if (phs2cmt == maxCmt)
				{	/* Wrap around and search again */
					if (index2)
					{
						phs2cmt = begCmt;
						maxCmt = &jpl->phase2_commit_array[index2];
						assert(currWriteaddr == phs2cmt->start_write_addr);
						PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt,	\
												maxCmt, currWriteaddr);
					}
					index1 = ((phs2cmt == maxCmt) ? index2 : (phs2cmt - begCmt));
				} else
					index1 = phs2cmt - begCmt;
			}
			if (phs2cmt < maxCmt)
				newWriteaddr = phs2cmt->start_write_addr;
			else
			{
				assert(phs2cmt == maxCmt);
				if (phs2cmt == begCmt)
				{	/* i.e. phs2cmt == begCmt == maxCmt. That is, begCmt and maxCmt are equal.
					 * Because of the "assert(begCmt < maxCmt)" assert in the "if (index1 < index2)" block
					 * above, we are guaranteed, we did not go down that code-block. In the else block,
					 * we did initialize topCmt so we can safely use that here.
					 */
					phs2cmt = topCmt;
				}
				phs2cmt--;
				newWriteaddr = phs2cmt->start_write_addr + phs2cmt->tot_jrec_len;
			}
			assert((index1 == index2) || (newWriteaddr == jpl->phase2_commit_array[index1].start_write_addr));
			SET_JPL_WRITE_ADDR(jpl, newWriteaddr);
			ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index1, JPL_PHASE2_COMMIT_ARRAY_SIZE);
			SHM_WRITE_MEMORY_BARRIER;
			jpl->phase2_commit_index1 = index1;
		}
	}
	if (!was_latch_owner)
		rel_latch(&jpl->phase2_commit_latch);
}
