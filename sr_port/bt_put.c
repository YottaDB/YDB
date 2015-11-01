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
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdscc.h"
#include "filestruct.h"
#include "gdsbgtr.h"
#include "send_msg.h"
#include "relqop.h"
#include "wcs_recover.h"
#include "wcs_get_space.h"
#include "io.h" 		/* for gtmsecshr.h */
#include "gtmsecshr.h"		/* for NORMAL_TERMINATION macro */

#ifdef DEBUG
static int4 entry_count=0;
#endif
GBLREF        uint4           process_id;

bt_rec_ptr_t bt_put(gd_region *r, int4 block)
{
	bt_rec_ptr_t		p, q0, q1, hdr;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	cache_rec_ptr_t		w;
	th_rec_ptr_t		th;
	trans_num		lcl_tn;
	uint4			lcnt;

	error_def(ERR_BTFAIL);
	error_def(ERR_THFAIL);
	error_def(ERR_WCFAIL);
	error_def(ERR_WCBLOCKED);

	csa = (sgmnt_addrs *)&FILE_INFO(r)->s_addrs;
	csd = csa->hdr;
	assert(csa->now_crit || csd->clustered);
	assert(dba_mm != csa->hdr->acc_meth);
	lcl_tn = csa->ti->curr_tn;
	hdr = csa->bt_header + (block % csd->bt_buckets);
	assert(BT_QUEHEAD == hdr->blk);
	for (lcnt = 0, p = (bt_rec_ptr_t)((sm_uc_ptr_t)hdr + hdr->blkque.fl);  ;
		p = (bt_rec_ptr_t)((sm_uc_ptr_t)p + p->blkque.fl), lcnt++)
	{
		if (BT_QUEHEAD == p->blk)
		{	/* there is no matching bt */
			assert(p == hdr);
			p = (bt_rec_ptr_t)((sm_uc_ptr_t)(csa->th_base) + csa->th_base->tnque.fl - sizeof(th->tnque));
			if (CR_NOTVALID != p->cache_index)
			{	/* the oldest bt is still valid */
				w = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, p->cache_index);
				if (w->dirty)
				{	/* get it written so it can be reused */
					BG_TRACE_PRO_ANY(csa, bt_put_flush_dirty);
					if (FALSE == wcs_get_space(r, 0, w))
					{
						if (FALSE == csd->wc_blocked)
						{
							assert(FALSE);
							secshr_db_clnup(NORMAL_TERMINATION);
							SET_TRACEABLE_VAR(csd->wc_blocked, TRUE);
							BG_TRACE_PRO_ANY(csa, wcb_bt_put);
							send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bt_put"),
								process_id, lcl_tn, DB_LEN_STR(r));
							return NULL;
						} else
							GTMASSERT;	/* we are in wcs_recover() already */
					}
				}
				p->cache_index = CR_NOTVALID;
				w->bt_index = 0;
			}
			q0 = (bt_rec_ptr_t)((sm_uc_ptr_t)p + p->blkque.fl);
			q1 = (bt_rec_ptr_t)remqt((que_ent_ptr_t)q0);
			if (EMPTY_QUEUE == (sm_long_t)q1)
				rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 1);
			p->blk = block;
			p->killtn = lcl_tn;
			insqt((que_ent_ptr_t)p, (que_ent_ptr_t)hdr);
			th = (th_rec_ptr_t)remqh((que_ent_ptr_t)csa->th_base);
			if (EMPTY_QUEUE == (sm_long_t)th)
				rts_error(VARLSTCNT(3) ERR_THFAIL, 1, 1);
			break;
		}
		if (p->blk == block)
		{
			q0 = (bt_rec_ptr_t)((sm_uc_ptr_t)p + p->tnque.fl);
			th = (th_rec_ptr_t)remqt((que_ent_ptr_t)((sm_uc_ptr_t)q0 + sizeof(th->tnque)));
			if (EMPTY_QUEUE == (sm_long_t)th)
				rts_error(VARLSTCNT(3) ERR_THFAIL, 1, 2);
			break;
		}
		if (0 == p->blkque.fl)
			rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 2);
		if (lcnt >= csd->n_bts)
			rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 3);
	}
	insqt((que_ent_ptr_t)th, (que_ent_ptr_t)csa->th_base);
	p->tn = lcl_tn;
	return p;
}
