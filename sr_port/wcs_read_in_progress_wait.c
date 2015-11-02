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
#include "interlock.h"
#include "gdsbgtr.h"
#include "sleep_cnt.h"
#include "wbox_test_init.h"

/* Include prototypes */
#include "wcs_sleep.h"
#include "is_proc_alive.h"
#include "wcs_read_in_progress_wait.h"
#include "add_inter.h"
#include "caller_id.h"

/* Waits for a concurrently running read (from disk into a global buffer) to complete.
 *
 * Returns TRUE if read completes within timeout of approx. 1 minute.
 * Returns FALSE otherwise.
 *
 * Similar logic is also present in t_qread and wcs_recover but they are different enough that
 * they have not been folded into this routine yet.
 */
boolean_t	wcs_read_in_progress_wait(cache_rec_ptr_t cr, wbtest_code_t wbox_test_code)
{
	uint4	lcnt, r_epid;
	int4	n;

	for (lcnt = 1; -1 != cr->read_in_progress; lcnt++)
	{
		if (-1 > cr->read_in_progress)
		{	/* outside of design; clear to known state */
			assert(0 == cr->r_epid);
			cr->r_epid = 0;
			INTERLOCK_INIT(cr);
			break;
		}
		wcs_sleep(lcnt);
		GTM_WHITE_BOX_TEST(wbox_test_code, lcnt, (2 * BUF_OWNER_STUCK));
		if (BUF_OWNER_STUCK < lcnt)
		{	/* sick of waiting */
			/* Since cr->r_epid can be changing concurrently, take a local copy before using it below,
			 * particularly before calling is_proc_alive as we dont want to call it with a 0 r_epid.
			 */
			r_epid = cr->r_epid;
			if (0 != r_epid)
			{
				if (FALSE == is_proc_alive(r_epid, cr->image_count))
				{	/* process gone; release its lock */
					cr->r_epid = 0;
					RELEASE_BUFF_READ_LOCK(cr);
				} else
				{
					assert(gtm_white_box_test_case_enabled);
					return FALSE;
				}
			} else
			{	/* process stopped before could set r_epid */
				RELEASE_BUFF_READ_LOCK(cr);	/* cr->r_epid already 0 no need to reset */
				if (-1 > cr->read_in_progress)
				{	/* process released since if (cr->r_epid); rectify semaphore  */
					LOCK_BUFF_FOR_READ(cr, n);
				}
			}
		}	/* sick of waiting */
	}
	return TRUE;
}
