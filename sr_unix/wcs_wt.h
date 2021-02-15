/****************************************************************
 *								*
 * Copyright (c) 2016-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef WCS_WT_H_INCLUDED
#define WCS_WT_H_INCLUDED

#include "sleep_cnt.h"
#include "wcs_sleep.h"

#define WT_LATCH_TIMEOUT_SEC    (4 * 60)        /* Define latch timeout as being 4 mins */

#define	REINSERT_CR_AT_TAIL(csr, ahead, n, csa, csd, trace_cntr)	\
MBSTART {								\
	n = INSQTI((que_ent_ptr_t)csr, (que_head_ptr_t)ahead);		\
	if (INTERLOCK_FAIL == n)					\
	{								\
		assert(FALSE);						\
		SET_TRACEABLE_VAR((cnl)->wc_blocked, WC_BLOCK_RECOVER);	\
		BG_TRACE_PRO_ANY(csa, trace_cntr);			\
	}								\
} MBEND

#define	BREAK_TWIN(CSR, CSA)											\
MBSTART {													\
	cache_rec_ptr_t		cr_new;										\
														\
	assert((CSR)->twin && (CSA)->now_crit); 	/* We need crit to break twin connections. */		\
	assert(!(CSR)->bt_index);	/* It has to be an OLDER twin. It cannot be a NEWER twin because	\
					 * as long as the OLDER twin exists in the WIP queue, the NEWER		\
					 * twin write would not have been issued by "wcs_wtstart".		\
					 */									\
	assert(!(CSR)->in_cw_set);	/* no other process should be needing this buffer */			\
	cr_new = (cache_rec_ptr_t)GDS_ANY_REL2ABS((CSA), (CSR)->twin); /* Get NEWER twin cr */			\
	assert((void *)&((cache_rec_ptr_t)GDS_ANY_REL2ABS((CSA), cr_new->twin))->state_que == (void *)(CSR));	\
	/* NEWER twin should be in ACTIVE queue, except in rare cases where processes were killed. */		\
	assert(cr_new->dirty || WBTEST_ENABLED(WBTEST_CRASH_SHUTDOWN_EXPECTED));				\
	(CSR)->cycle++;	/* increment cycle whenever blk number changes (tp_hist needs it) */			\
	(CSR)->blk = CR_BLKEMPTY;										\
	assert(CR_BLKEMPTY != cr_new->blk);	/* NEWER twin should have a valid block number */		\
	cr_new->twin = (CSR)->twin = 0;	/* Break the twin link */						\
	cr_new->backup_cr_is_twin = FALSE;									\
} MBEND
/* "wcs_wtfini" is called with a second parameter which indicates whether it has to do "is_proc_alive" check or not.
 * In places where we know for sure we do not need this check, we pass FALSE. In places where we would benefit from a check
 * we pass TRUE but since "wcs_wtfini" is usually called in a "wcs_sleep" loop using "lcnt" variable iterating from 1 to
 * SLEEP_ONE_MIN/BUF_OWNER_STUCK, we want to limit the heavyweight nature of the "is_proc_alive" check by doing it only
 * 32 times for every SLEEP_ONE_MIN iterations of the loop. Hence the MAX/32 calculation below. This approximates to
 * doing the system call once every 2 seconds.
 */
#define	CHECK_IS_PROC_ALIVE_FALSE			FALSE
#define	CHECK_IS_PROC_ALIVE_TRUE			TRUE
#define	CHECK_IS_PROC_ALIVE_TRUE_OR_FALSE(lcnt, MAX)	(0 == (lcnt % (MAX/32)))

#ifdef DEBUG
enum dbg_wtfini_lcnt_t {
	dbg_wtfini_db_csh_getn = 32768,	/* a value greater than SLEEP_ONE_MIN (= 6000) and UNIX_GETSPACEWAIT (= 12000)
					 * to distinguish this from other "lcnt".
					 */
	dbg_wtfini_wcs_recover = 32769,
	dbg_wtfini_wcs_get_space1 = 32770,
	dbg_wtfini_wcs_get_space2 = 32771,
	dbg_wtfini_wcs_wtstart = 32772,
	dbg_wtfini_wcs_get_space3 = 32773
};

GBLREF	enum dbg_wtfini_lcnt_t	dbg_wtfini_lcnt;	/* "lcnt" value for WCS_OPS_TRACE tracking purposes */
#endif

typedef struct  wtstart_cr_list_struct
{
	int numcrs;
	int listsize; /* number of items allocated for listcrs[] */
	cache_rec_ptr_t *listcrs;
} wtstart_cr_list_t;

int		wcs_wt_restart(unix_db_info *udi, cache_state_rec_ptr_t csr);
int		wcs_wtfini(gd_region *reg, boolean_t do_is_proc_alive_check, cache_rec_ptr_t cr2flush);
void		wcs_wtfini_nocrit(gd_region *reg, wtstart_cr_list_t *cr_list_ptr);
void		wcs_wterror(gd_region *reg, int4 save_errno);
int4		wcs_wtstart(gd_region *region, int4 writes, wtstart_cr_list_t *cr_list_ptr, cache_rec_ptr_t cr2flush);
int		wcs_wtstart_fini(gd_region *region, int nbuffs, cache_rec_ptr_t cr2flush);

error_def(ERR_AIOQUEUESTUCK);
error_def(ERR_DBFILERR);

/* Wait for wip queue to be cleared or a specific CR to become non-dirty. RET is non-zero if "wcs_wtfini"
 * returns a non-zero value in any iteration. We wait a max of 1 minute if we do not see any progress in
 * the WIP queue count. If there is evidence of progress, time waited till now does not count towards the 1
 * minute timeout i.e. the 1 minute wait is started afresh. If we hit a minute we send a message to the syslog
 * and keep trying.
 */
inline static int wait_for_wip_queue_to_clear(node_local_ptr_t cnl, cache_que_head_ptr_t crwipq, cache_rec_ptr_t cr, gd_region *reg)
{
	int		ret;
	unsigned int	lcnt;
	int		active_cnt, wip_cnt;

	ret = 0; /* initialize ret in case we do not invoke "wcs_wtfini" below */
	DEBUG_ONLY(active_cnt = cnl->wcs_active_lvl;)	/* for debugging purposes */
	wip_cnt = cnl->wcs_wip_lvl;
	for (lcnt = 1; ; lcnt++)
	{
		if (!crwipq->fl)
			break;
		DEBUG_ONLY(dbg_wtfini_lcnt = lcnt);     /* used by "wcs_wtfini" */
		ret = wcs_wtfini(reg, CHECK_IS_PROC_ALIVE_TRUE_OR_FALSE(lcnt, MAX_WIP_QWAIT), cr);
		if (ret || (cr && (0 == cr->dirty)))
			break;
		if (0 == (lcnt = (lcnt % (MAX_WIP_QWAIT AIX_ONLY(* 4)))))
		{
			send_msg_csa(CSA_ARG(REG2CSA(reg)) VARLSTCNT(7) ERR_DBFILERR, 2, DB_LEN_STR(reg),
					ERR_AIOQUEUESTUCK, 2, (lcnt / SLEEP_ONE_MIN), (cr ? cr->blk : 0));
		}
		wcs_sleep(lcnt);
		if ((wip_cnt != cnl->wcs_wip_lvl) && (NULL == cr)) /* only for non-specific crs */
		{	/* Change in WIP queue size. Restart wait. Note that "CNL->wcs_wip_lvl" could
			 * have even increased since we last noted it in "wip_cnt". This is because
			 * a concurrent process in "wcs_wtstart" could have moved some more records
			 * from the active queue to the wip queue. Restart wait even in that case.
			 */
			lcnt = 1;
			wip_cnt = cnl->wcs_wip_lvl;
		}
	}

	return ret;
}

#endif
