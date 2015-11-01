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

#include "mdef.h"

#ifdef UNIX
#include "aswp.h"
#endif
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cdb_sc.h"
#include "copy.h"
#include "error.h"
#include "gdscc.h"
#include "interlock.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "io.h"			/* for gtmsecshr.h */
#include "gtmsecshr.h"		/* for NORMAL_TERMINATION macro */
#include "t_commit_cleanup.h"
#ifdef UNIX
#include "process_deferred_stale.h"
#endif

GBLREF uint4		t_err;
GBLREF unsigned char	cw_set_depth;
GBLREF cw_set_element	cw_set[];
GBLREF sgmnt_addrs	*cs_addrs;
GBLREF sgmnt_data_ptr_t	cs_data;
GBLREF gd_region	*gv_cur_region;
GBLREF unsigned int	t_tries;
GBLREF short		dollar_tlevel;
GBLREF sgm_info		*first_sgm_info;
GBLREF cache_rec_ptr_t	cr_array[((MAX_BT_DEPTH * 2) - 1) * 2];	/* Maximum number of blocks that can be in transaction */
GBLREF unsigned int	cr_array_index;
GBLREF boolean_t        unhandled_stale_timer_pop;

#define	CACHE_REC_CLEANUP(cr)			\
	assert(!cr->in_tend);			\
	assert(!cr->data_invalid);		\
	cr->in_cw_set = FALSE;

boolean_t t_commit_cleanup(enum cdb_sc status, int signal)
{
	boolean_t	update_underway;
	cache_rec_ptr_t	cr;
	sgm_info	*si;
      	cw_set_element	*first_cse;

	error_def(ERR_TPFAIL);

	assert(dollar_tlevel || cs_addrs->now_crit);
	assert(cdb_sc_normal != status);
	cs_addrs->ti->curr_tn = cs_addrs->ti->early_tn;

	update_underway = FALSE;
	if (dollar_tlevel > 0)
	{
		for (si = first_sgm_info;  NULL != si;  si = si->next_sgm_info)
		{
			TP_CHANGE_REG(si->gv_cur_region);
      			first_cse = si->first_cw_set;
      			TRAVERSE_TO_LATEST_CSE(first_cse);
      			if (NULL != first_cse)
			{
				if (cs_addrs->t_commit_crit || gds_t_committed == first_cse->mode)
				{
					update_underway = TRUE;
					t_err = ERR_TPFAIL;
				}
				break;
			}
		}
	} else
		update_underway = cs_addrs->t_commit_crit;
	if (!update_underway)
	{
		if (dollar_tlevel > 0)
		{
			for (si = first_sgm_info;  NULL != si;  si = si->next_sgm_info)
			{
				TP_CHANGE_REG(si->gv_cur_region);
				while (si->cr_array_index > 0)
				{
					cr = si->cr_array[--si->cr_array_index];
					CACHE_REC_CLEANUP(cr);
				}
				cs_addrs->t_commit_crit = FALSE;
				if (t_tries < CDB_STAGNATE)
					rel_crit(gv_cur_region);
			}
		} else
		{
			while (cr_array_index > 0)
			{
				cr = cr_array[--cr_array_index];
				CACHE_REC_CLEANUP(cr);
			}
			cs_addrs->t_commit_crit = FALSE;
			if (t_tries < CDB_STAGNATE)
				rel_crit(gv_cur_region);
		}
		UNIX_ONLY(
			if ((t_tries < CDB_STAGNATE) && unhandled_stale_timer_pop)
				process_deferred_stale();
		)
	} else
	{
		secshr_db_clnup(NORMAL_TERMINATION);
		UNIX_ONLY(
			if (unhandled_stale_timer_pop)
				process_deferred_stale();
		)
	}
	return update_underway;
}
