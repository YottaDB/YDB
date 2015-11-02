/****************************************************************
 *								*
 *	Copyright 2007, 2011 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "interlock.h"
#include "gdsbgtr.h"
#include "sleep_cnt.h"
#include "wbox_test_init.h"
#include "copy.h"

/* Include prototypes */
#include "caller_id.h"
#include "send_msg.h"
#include "wcs_sleep.h"
#include "is_proc_alive.h"
#include "wcs_write_in_progress_wait.h"
#include "add_inter.h"
#include "gtm_c_stack_trace.h"

GBLREF	gd_region	*gv_cur_region;	/* for the LOCK_HIST macro used in LOCK_BUFF_FOR_UPDATE macro */
GBLREF	uint4		process_id;	/* for the LOCK_HIST macro used in LOCK_BUFF_FOR_UPDATE macro */

error_def (ERR_WRITEWAITPID);

/* Waits for a concurrently running write (of a global buffer to disk) to complete.
 *
 * Returns TRUE if write completes within timeout of approx. 1 minute.
 * Returns FALSE otherwise.
 */
boolean_t	wcs_write_in_progress_wait(node_local_ptr_t cnl, cache_rec_ptr_t cr, wbtest_code_t wbox_test_code)
{
	uint4	lcnt;
	int4	n;


	for (lcnt = 1; ; lcnt++)
	{	/* the design here is that either this process owns the block, or the writer does.
		 * if the writer does, it must be allowed to finish its write; then it will release the block
		 * and the next LOCK will establish ownership
		 */
		LOCK_BUFF_FOR_UPDATE(cr, n, &cnl->db_latch);
		/* This destroys evidence of writer ownership, but this is really a test that
		 * there was no prior owner. It will only be true if the writer has cleared it.
		 */
		if (OWN_BUFF(n))
			break;
		else
		{
			GTM_WHITE_BOX_TEST(wbox_test_code, lcnt, (2 * BUF_OWNER_STUCK));
			/* We have noticed the below assert to fail occasionally on some platforms
			 * We suspect it is because of waiting for another writer that is in jnl_fsync
			 * (as part of flushing a global buffer) which takes more than a minute to finish.
			 * To avoid false failures (where the other writer finishes its job in a little over
			 * a minute) we wait for twice the time in the debug version.
			 */
DEBUG_ONLY(
			if ((BUF_OWNER_STUCK == lcnt) && cr->epid)
				GET_C_STACK_FROM_SCRIPT("WRITEWAITPID", process_id, cr->epid, ONCE);
	  )
			if (BUF_OWNER_STUCK DEBUG_ONLY( * 2) < lcnt)
			{	/* sick of waiting */
				if (0 == cr->dirty)
				{	/* someone dropped something; assume it was the writer and go on */
					LOCK_NEW_BUFF_FOR_UPDATE(cr);
					break;
				} else
				{
					if (cr->epid)
					{
#ifdef DEBUG
						GET_C_STACK_FROM_SCRIPT("WRITEWAITPID", process_id, cr->epid, TWICE);
						send_msg(VARLSTCNT(8) ERR_WRITEWAITPID, 6, process_id, TWICE, \
							cr->epid, cr->blk, DB_LEN_STR(gv_cur_region));
#else
						GET_C_STACK_FROM_SCRIPT("WRITEWAITPID", process_id, cr->epid, ONCE);
						send_msg(VARLSTCNT(8) ERR_WRITEWAITPID, 6, process_id, ONCE, \
							cr->epid, cr->blk, DB_LEN_STR(gv_cur_region));
#endif
					}
					return FALSE;
				}
			}
			if (WRITER_STILL_OWNS_BUFF(cr, n))
				wcs_sleep(lcnt);
		}
	}	/* end of for loop to control buffer */
	return TRUE;
}
