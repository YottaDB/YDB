/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <sys/shm.h>

#include "mdef.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mlkdef.h"
#include "lockdefs.h"
#include "interlock.h"
#include "sleep.h"
#include "wbox_test_init.h"
#include "do_shmat.h"

/* Include prototypes */
#include "mlk_ops.h"
#include "mlk_prcblk_delete.h"
#include "mlk_shrblk_delete_if_empty.h"
#include "mlk_shrclean.h"
#include "is_proc_alive.h"

GBLREF uint4	process_id;

#define PROC_TABLE_SIZE 16384

typedef struct
{
        pid_t pid;
        unsigned int pstart;
} pidandstart_t;

void fill_pid_table(mlk_shrblk_ptr_t d, pidandstart_t *table, mlk_pvtctl_ptr_t pctl);
void check_pids(pidandstart_t *table, char *dead_table, sgmnt_addrs *csa);
void clean_pids(mlk_shrblk_ptr_t d, pidandstart_t *pid_table, char *dead_table, mlk_pvtctl_ptr_t pctl);

/**
 * Clears the orphaned locks in region
 *
 * @param region
 * @param ctl shared memory segment corresponding to the lock table
 */
void mlk_shrclean(mlk_pvtctl_ptr_t pctl)
{
	boolean_t	was_crit;
	char		dead_table[PROC_TABLE_SIZE];
	pidandstart_t	pid_table[PROC_TABLE_SIZE];

	was_crit = LOCK_CRIT_HELD(pctl->csa);
	assert(was_crit && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	assert(pctl->ctl->lock_gc_in_progress.u.parts.latch_pid == process_id);
	memset(pid_table, 0, SIZEOF(pidandstart_t) * PROC_TABLE_SIZE);
	memset(dead_table, 0, SIZEOF(char) * PROC_TABLE_SIZE);
	if (!pctl->ctl->blkroot)
		return;
	fill_pid_table((mlk_shrblk_ptr_t)R2A(pctl->ctl->blkroot), pid_table, pctl);
	REL_LOCK_CRIT(pctl, was_crit);
	check_pids(pid_table, dead_table, pctl->csa);
	GRAB_LOCK_CRIT_AND_SYNC(pctl, was_crit);
	if (pctl->ctl->blkroot)
		clean_pids((mlk_shrblk_ptr_t)R2A(pctl->ctl->blkroot), pid_table, dead_table, pctl);
	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	return;
}

/**
 * Scans through the siblings of d, and children of siblings and d, noting down each PID in the shrblk's and pcrblk's in table.
 *
 * @param [in] d shrblk to start scanning at
 * @param [out] table which will contain the PID's currently holding locks or on the queue
 * @param [in] ctl pointer to the lock table control structure
 */
void fill_pid_table(mlk_shrblk_ptr_t d, pidandstart_t *table, mlk_pvtctl_ptr_t pctl)
{
	int4			index;
	mlk_prcblk_ptr_t	p;
	mlk_shrblk_ptr_t	d2;

	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	d2 = d;
	if (d2->rsib)
		d2 = (mlk_shrblk_ptr_t)R2A(d2->rsib);
	do {
		if (d2->rsib)
		{
			CHECK_SHRBLKPTR(d2->rsib, *pctl);
			d2 = (mlk_shrblk_ptr_t)R2A(d2->rsib);
			if (d2->rsib)
			{
				CHECK_SHRBLKPTR(d2->rsib, *pctl);
				d2 = (mlk_shrblk_ptr_t)R2A(d2->rsib);
			}
		}
		CHECK_SHRBLKPTR(d->rsib, *pctl);
		if (d->children)
			fill_pid_table((mlk_shrblk_ptr_t)R2A(d->children), table, pctl);
		if (d->owner)
		{
			index = d->owner % PROC_TABLE_SIZE;
			table[index].pid = d->owner;
			table[index].pstart = d->pstart;
		}
		for (p = d->pending ? (mlk_prcblk_ptr_t)R2A(d->pending) : 0; p && p->next; p = (mlk_prcblk_ptr_t)R2A(p->next))
		{
			index = p->process_id % PROC_TABLE_SIZE;
			table[index].pid = p->process_id;
			table[index].pstart = p->process_start;
		}
		if (d2 == d)
			break;
	} while (d->rsib && (d = (mlk_shrblk_ptr_t)R2A(d->rsib)));
}

/**
 * Issues KILL -0's to each process in table to see if they are alive, and populates dead_table with the result.
 * Runs without lock crit since it is potentially long-running.
 *
 * @param [in] pid_table list of PID's to check, or 0 if no PID is at a given slot
 * @param [out] dead_table will contain 1 in dead_table[i] if the table[i] is dead
 * @param [in] csa used to verify we don't hold lock crit
*/
void check_pids(pidandstart_t *pid_table, char *dead_table, sgmnt_addrs *csa)
{
	uint4	i;

	assert(INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state);
	for (i = 0; i < PROC_TABLE_SIZE; i++)
	{
		if (pid_table[i].pid != 0)
		{
			dead_table[i] = !is_proc_alive(pid_table[i].pid, pid_table[i].pstart);
		}
	}
	WBTEST_ONLY(WBTEST_MLOCK_HANG_AFTER_SCAN, SLEEP_USEC(10000ULL * MILLISECS_IN_SEC, 0););
}

void clean_pids(mlk_shrblk_ptr_t d, pidandstart_t *pid_table, char *dead_table, mlk_pvtctl_ptr_t pctl)
{
	boolean_t		deleted;
	int4			index;
	mlk_prcblk_ptr_t	p;
	mlk_shrblk_ptr_t	cur, first, next;

	assert(LOCK_CRIT_HELD(pctl->csa) && (INTRPT_IN_MLK_SHM_MODIFY == intrpt_ok_state));
	cur = first = d;
	do {
		assertpro(cur->rsib != 0);
		CHECK_SHRBLKPTR(cur->rsib, *pctl);
		next = (mlk_shrblk_ptr_t)R2A(cur->rsib);
		if (cur->children)
			clean_pids((mlk_shrblk_ptr_t)R2A(cur->children), pid_table, dead_table, pctl);
		for (p = cur->pending ? (mlk_prcblk_ptr_t)R2A(cur->pending) : 0; p && p->next; p = (mlk_prcblk_ptr_t)R2A(p->next))
		{
			index = p->process_id % PROC_TABLE_SIZE;
			if (p->process_id && ((pid_table[index].pid == p->process_id) && dead_table[index]))
			{
				p->process_id = 0;
				p->process_start = 0;
				p->ref_cnt = 0;
			}
		}
		mlk_prcblk_delete(pctl, cur, 0);
		index = cur->owner % PROC_TABLE_SIZE;
		if (cur->owner == 0 || ((pid_table[index].pid == cur->owner) && dead_table[index]))
		{
			assertpro(cur->lsib != INVALID_LSIB_MARKER);
			cur->owner = 0;
			cur->pstart = 0;
			cur->sequence = pctl->csa->hdr->trans_hist.lock_sequence++;
			deleted = mlk_shrblk_delete_if_empty(pctl, cur);
		} else
			deleted = FALSE;
		if (next == first)
			break;		/* We just did the last one, so we are done. */
		if (deleted && (cur == first))
			first = next;	/* We just deleted the first one, so make the next one the first one. */
		cur = next;
	} while(TRUE);
}
