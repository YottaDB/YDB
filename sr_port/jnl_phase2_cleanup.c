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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "jnl.h"
#include "memcoherency.h"
#include "interlock.h"
#include "is_proc_alive.h"
#include "gdsbgtr.h"

GBLREF	uint4		process_id;

#define	PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt)	\
MBSTART {										\
	assert(phs2cmt < maxCmt);							\
	do										\
	{										\
		if (!phs2cmt->write_complete)						\
			break;								\
		phs2cmt++;								\
		if (phs2cmt == maxCmt)							\
			break;								\
	} while (TRUE);									\
} MBEND

/* The below code is very similar to "repl_phase2_cleanup" */
void	jnl_phase2_cleanup(sgmnt_addrs *csa, jnl_buffer_ptr_t jbp)
{
	uint4			currFreeaddr, newFreeaddr, stuckPid;
	int			index1, index2;
	jbuf_phase2_in_prog_t	*phs2cmt, *deadCmt, *begCmt, *maxCmt, *topCmt;
	boolean_t		was_latch_owner;
	DEBUG_ONLY(uint4	jbp_freeaddr1;)

	/* It is possible we already own the latch in case we are in timer-interrupt or process-exit code hence the below check */
	was_latch_owner = GLOBAL_LATCH_HELD_BY_US(&jbp->phase2_commit_latch);
	if (!was_latch_owner)
	{	/* Return value of "grab_latch" does not need to be checked because we pass GRAB_LATCH_INDEFINITE_WAIT as timeout */
		grab_latch(&jbp->phase2_commit_latch, GRAB_LATCH_INDEFINITE_WAIT);
	}
	index1 = jbp->phase2_commit_index1;
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index1, JNL_PHASE2_COMMIT_ARRAY_SIZE);
	index2 = jbp->phase2_commit_index2;
	SHM_READ_MEMORY_BARRIER;	/* see corresponding SHM_WRITE_MEMORY_BARRIER in UPDATE_JBP_RSRV_FREEADDR macro */
	ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index2, JNL_PHASE2_COMMIT_ARRAY_SIZE);
	if (index1 != index2)
	{
		DEBUG_ONLY(jbp_freeaddr1 = jbp->freeaddr); /* note down to help with assert failure in SET_JBP_FREEADDR below */
		phs2cmt = &jbp->phase2_commit_array[index1];
		if (!phs2cmt->write_complete && csa->now_crit)
		{	/* "jnl_phase2_write" not complete in earliest phase2 entry. Since that is blocking us in crit,
			 * check if the pid is alive. If not, cleanup. If you find the first blocking pid is dead, check
			 * the next few pids too until an alive pid or phase2-complete entry shows up.
			 */
			deadCmt = phs2cmt;
			assert((deadCmt->start_freeaddr <= jbp->freeaddr)
				&& ((deadCmt->start_freeaddr + deadCmt->tot_jrec_len) > jbp->freeaddr));
			if (index1 < index2)
				maxCmt = &jbp->phase2_commit_array[index2];
			else
				maxCmt = &jbp->phase2_commit_array[JNL_PHASE2_COMMIT_ARRAY_SIZE];
			assert(deadCmt < maxCmt);
			do
			{
				if (deadCmt->write_complete)
					break;
				stuckPid = deadCmt->process_id;
				/* Note that we can reach here with stuckPid == process_id. An example call graph follows
				 *	t_end -> jnl_write_phase2 -> jnl_write_pblk -> jnl_write -> jnl_write_attempt
				 *									-> jnl_phase2_cleanup
				 * Therefore "break" in that case.
				 */
				if (stuckPid == process_id)
					break;
				BG_TRACE_PRO_ANY(csa, jnlbuff_phs2cmt_isprcalv);
				if (is_proc_alive(stuckPid, 0))
					break;
				jnl_phase2_salvage(csa, jbp, deadCmt);
				assert(deadCmt->write_complete);
			} while (++deadCmt < maxCmt);
			/* If "(deadCmt == maxCmt) && (index < index2)", we do not wrap around and search for dead pids in
			 * the beginning of the array. Instead the next call to "jnl_phase2_cleanup" will take care of
			 * cleaning that section IF there are any deadCmt entries.
			 */
		}
		if (phs2cmt->write_complete)
		{	/* There is at least one phase2 that is complete and can be cleaned up.
			 * Note that jbp->freeaddr will mostly be equal to phs2cmt->start_freeaddr but in rare cases
			 * it can be greater. This is possible for example if a TP transaction has total jnl records
			 * written that is greater than the jnlbuffer size. In that case, jb->freeaddr is updated as
			 * each jnlbuffer size is filled even though all jnl records of the transaction are not yet written.
			 */
			assert(jbp->freeaddr >= phs2cmt->start_freeaddr);
			begCmt = &jbp->phase2_commit_array[0];
			if (index1 < index2)
			{	/* Easier case. No wrapping needed. */
				maxCmt = &jbp->phase2_commit_array[index2];
				assert(begCmt < maxCmt);
				PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt);
				index1 = phs2cmt - begCmt;
			} else
			{
				maxCmt = topCmt = &jbp->phase2_commit_array[JNL_PHASE2_COMMIT_ARRAY_SIZE];
				PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt);
				if (phs2cmt == maxCmt)
				{	/* Wrap around and search again */
					if (index2)
					{
						phs2cmt = begCmt;
						maxCmt = &jbp->phase2_commit_array[index2];
						PHASE2_COMMIT_ARRAY_ITERATE_UNTIL_WRITE_COMPLETE_FALSE(csa, phs2cmt, maxCmt);
					}
					index1 = ((phs2cmt == maxCmt) ? index2 : (phs2cmt - begCmt));
				} else
					index1 = phs2cmt - begCmt;
			}
			if (phs2cmt < maxCmt)
				newFreeaddr = phs2cmt->start_freeaddr;
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
				newFreeaddr = phs2cmt->start_freeaddr + phs2cmt->tot_jrec_len;
			}
			assert(!csa->now_crit || (index1 == index2)
					|| (newFreeaddr == jbp->phase2_commit_array[index1].start_freeaddr));
			SET_JBP_FREEADDR(jbp, newFreeaddr);
			ASSERT_JNL_PHASE2_COMMIT_INDEX_IS_VALID(index1, JNL_PHASE2_COMMIT_ARRAY_SIZE);
			SHM_WRITE_MEMORY_BARRIER;	/* Update phase2_commit_index1 after memory barrier since
							 * jnl_write_attempt relies on jbp->freeaddr & jbp->free being
							 * reliable once phase2_commit_index1 is updated.
							 */
			jbp->phase2_commit_index1 = index1;
		}
	}
	if (!was_latch_owner)
		rel_latch(&jbp->phase2_commit_latch);
}
