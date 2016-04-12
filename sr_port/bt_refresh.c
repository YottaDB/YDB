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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "longset.h"
#include "relqop.h"

/* Refresh the database cache records in the shared memory. If init = TRUE, do the longset and initialize all the forward and
 * backward links. If init = FALSE, reset only the needed fields.
 */
void bt_refresh(sgmnt_addrs *csa, boolean_t init)
{
	sgmnt_data_ptr_t	csd;
	bt_rec_ptr_t		ptr, top, bt1;


	csd = csa->hdr;
	assert(dba_bg == csd->acc_meth);
	if (init)
		longset((uchar_ptr_t)csa->bt_header, (csd->bt_buckets + csd->n_bts + 1) * SIZEOF(bt_rec), 0);

	for (ptr = csa->bt_header, top = ptr + csd->bt_buckets + 1; ptr < top; ptr++)
		ptr->blk = BT_QUEHEAD;

	for (ptr = csa->bt_base, bt1 = csa->bt_header, top = ptr + csd->n_bts; ptr < top ; ptr++, bt1++)
	{
		ptr->blk = BT_NOTVALID;
		ptr->cache_index = CR_NOTVALID;
		ptr->tn = ptr->killtn = 0;
		if (init)
		{
			insqt((que_ent_ptr_t)ptr, (que_ent_ptr_t)bt1);
			insqt((que_ent_ptr_t)((sm_uc_ptr_t)ptr + (2 * SIZEOF(sm_off_t))), (que_ent_ptr_t)csa->th_base);
		}
	}
	SET_OLDEST_HIST_TN(csa, csa->ti->curr_tn - 1);
	csa->ti->mm_tn = 0;
	return;
}
