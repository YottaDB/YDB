/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "t_begin_crit.h"
#include "longset.h"		/* needed for cws_insert.h */
#include "hashtab_int4.h"	/* needed for cws_insert.h */
#include "cws_insert.h"

GBLDEF	srch_hist		dummy_hist;

GBLREF	gd_region		*gv_cur_region;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	trans_num		start_tn;
GBLREF	uint4			t_err;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	uint4			update_trans;
GBLREF	boolean_t		write_after_image;
GBLREF	volatile int4		fast_lock_count;

void	t_begin_crit(uint4 err)	/* err - error code for current gvcst_routine */
{
	boolean_t	was_crit;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	CWS_RESET;
	start_tn = cs_addrs->ti->curr_tn;
	cw_set_depth = 0;
	t_tries = CDB_STAGNATE;
	/* since this is mainline code and we know fast_lock_count should be 0 at this point reset it just in case it is not.
	 * having fast_lock_count non-zero will defer the database flushing logic and other critical parts of the system.
	 * hence this periodic reset at the beginning of each transaction.
	 */
	assert(0 == fast_lock_count);
	fast_lock_count = 0;
	t_err = err;
	if (non_tp_jfb_ptr)
		non_tp_jfb_ptr->record_size = 0; /* re-initialize it to 0 since TOTAL_NONTPJNL_REC_SIZE macro uses it */
	/* the only currently known callers of this routine are DSE and MUPIP RECOVER (mur_put_aimg_rec.c).
	 * all of them set "write_after_image" to TRUE. hence the assert below.
	 */
	assert(write_after_image);
	update_trans = UPDTRNS_DB_UPDATED_MASK;
	was_crit = cs_addrs->now_crit;
	assert(!was_crit || cs_addrs->hold_onto_crit);
	if (!was_crit)
	{	/* We are going to grab_crit. If csa->nl->wc_blocked is set to TRUE, we will end up calling wcs_recover as part of
		 * grab_crit. Set variable to indicate it is ok to do so even though t_tries is CDB_STAGNATE since we are not
		 * in the middle of any transaction.
		 */
		DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = TRUE;)
		grab_crit(gv_cur_region);
		DEBUG_ONLY(TREF(ok_to_call_wcs_recover) = FALSE;)
	}
}
