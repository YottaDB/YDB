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
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdsbgtr.h"
#include "ccp.h"
#include "locks.h"
#include <lckdef.h>
#include <psldef.h>
#include <efndef.h>

#include "jnl.h"


static	int4	delta_50_msec[2] = { -500000, -1 };	/**** PUT THIS IN THE DATA BASE HEADER !!! ***/


/* Exit write mode and wait for buffers to be written out */

void	ccp_tr_ewmwtbf( ccp_action_record *rec)
{
	ccp_db_header	*db;
	gd_region	*r;
	sgmnt_addrs	*csa;
	cache_rec	*w, *wtop;
	uint4	status;


	db = rec->v.h;
	assert(db->segment->nl->ccp_state == CCST_WMXGNT);

	r = db->greg;
	wcs_wtfini(r);
	sys$dclast(wcs_wtstart, r, 0);
	csa = &FILE_INFO(r)->s_addrs;

	w = &db->segment->acc_meth.bg.cache_state->cache_array;
	w += db->segment->hdr->bt_buckets;
	for (wtop = w + db->glob_sec->n_bts; (w < wtop) && (0 == w->dirty); ++w)
		;

	if (w >= wtop  &&  db->segment->acc_meth.bg.cache_state->cacheq_active.fl == 0  &&
	    (!JNL_ENABLED(csa->hdr)  ||  csa->jnl == NULL  ||  csa->jnl->jnl_buff->dskaddr == csa->jnl->jnl_buff->freeaddr))
	{
		/* All dirty buffers have now been flushed */
		(void)ccp_enqw(EFN$C_ENF, LCK$K_NLMODE, &db->flush_iosb, LCK$M_CONVERT, NULL, 0, NULL, 0, NULL, PSL$C_USER, 0);
		/***** Check error status here? *****/

		db->segment->nl->ccp_state = CCST_RDMODE;
		db->write_mode_requested = FALSE;

		ccp_pndg_proc_wake(&db->exitwm_wait);

		if (db->close_region)
			ccp_close1(db);
		else
			if (db->write_wait.first != NULL  ||  db->flu_wait.first != NULL)
				ccp_request_write_mode(db);
	}
	else
		if ((status = sys$setimr(0, delta_50_msec, ccp_ewmwtbf_interrupt, &db->exitwm_timer_id, 0)) != SS$_NORMAL)
			ccp_signal_cont(status);	/***** Is this reasonable? *****/

}
