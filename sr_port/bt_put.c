/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "jnl.h"
#include "wbox_test_init.h"

#ifdef DEBUG
static	int4		entry_count = 0;
#endif

GBLREF	volatile boolean_t	in_wcs_recover;	/* TRUE if in "wcs_recover" */
GBLREF	uint4			process_id;
GBLREF 	jnl_gbls_t		jgbl;

error_def(ERR_BTFAIL);
error_def(ERR_WCBLOCKED);

bt_rec_ptr_t bt_put(gd_region *reg, int4 block)
{
	bt_rec_ptr_t		bt, q0, q1, hdr;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	cache_rec_ptr_t		cr;
	th_rec_ptr_t		th;
	trans_num		lcl_tn;
	uint4			lcnt;

	csa = (sgmnt_addrs *)&FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(csa->now_crit || csd->clustered);
	assert(dba_mm != csa->hdr->acc_meth);
	lcl_tn = csa->ti->curr_tn;
	hdr = csa->bt_header + (block % csd->bt_buckets);
	assert(BT_QUEHEAD == hdr->blk);
	for (lcnt = 0, bt = (bt_rec_ptr_t)((sm_uc_ptr_t)hdr + hdr->blkque.fl);  ;
		bt = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.fl), lcnt++)
	{
		if (BT_QUEHEAD == bt->blk)
		{	/* there is no matching bt */
			assert(bt == hdr);
			bt = (bt_rec_ptr_t)((sm_uc_ptr_t)(csa->th_base) + csa->th_base->tnque.fl - SIZEOF(th->tnque));
			if (CR_NOTVALID != bt->cache_index)
			{	/* the oldest bt is still valid */
				assert(!in_wcs_recover);
				cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				if (cr->dirty)
				{	/* get it written so it can be reused */
					BG_TRACE_PRO_ANY(csa, bt_put_flush_dirty);
					if (FALSE == wcs_get_space(reg, 0, cr))
					{
						assert(csa->nl->wc_blocked);	/* only reason we currently know
										 * why wcs_get_space could fail */
						assert(gtm_white_box_test_case_enabled);
						BG_TRACE_PRO_ANY(csa, wcb_bt_put);
						send_msg(VARLSTCNT(8) ERR_WCBLOCKED, 6, LEN_AND_LIT("wcb_bt_put"),
							process_id, &lcl_tn, DB_LEN_STR(reg));
						return NULL;
					}
				}
				bt->cache_index = CR_NOTVALID;
				cr->bt_index = 0;
			}
			q0 = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.fl);
			q1 = (bt_rec_ptr_t)remqt((que_ent_ptr_t)q0);
			if (EMPTY_QUEUE == (sm_long_t)q1)
				rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 1);
			bt->blk = block;
			bt->killtn = lcl_tn;
			insqt((que_ent_ptr_t)bt, (que_ent_ptr_t)hdr);
			th = (th_rec_ptr_t)remqh((que_ent_ptr_t)csa->th_base);
			assertpro(EMPTY_QUEUE != (sm_long_t)th);
			break;
		}
		if (bt->blk == block)
		{	/* bt_put should never be called twice for the same block with the same lcl_tn. This is because
			 * t_end/tp_tend update every block only once as part of each update transaction. Assert this.
			 * The two exceptions are
			 *   a) Forward journal recovery which simulates a 2-phase M-kill where the same block
			 *	could get updated in both phases (example bitmap block gets updated for blocks created
			 *	within the TP transaction as well as for blocks that are freed up in the 2nd phase of
			 *	the M-kill) with the same transaction number. This is because although GT.M would have
			 *	updated the same block with different transaction numbers in the two phases, forward
			 *	recovery will update it with the same tn and instead increment the db tn on seeing the
			 *	following INCTN journal record(s).
			 *   b) Cache recovery (wcs_recover). It could call bt_put more than once for the same block
			 *	and potentially with the same tn. This is because the state of the queues is questionable
			 *	and there could be more than one cache record for a given block number.
			 */
			assert(in_wcs_recover || (bt->tn < lcl_tn) || (jgbl.forw_phase_recovery && !JNL_ENABLED(csa)));
			q0 = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->tnque.fl);
			th = (th_rec_ptr_t)remqt((que_ent_ptr_t)((sm_uc_ptr_t)q0 + SIZEOF(th->tnque)));
			assertpro(EMPTY_QUEUE != (sm_long_t)th);
			break;
		}
		if (0 == bt->blkque.fl)
			rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 2);
		if (lcnt >= csd->n_bts)
			rts_error(VARLSTCNT(3) ERR_BTFAIL, 1, 3);
	}
	insqt((que_ent_ptr_t)th, (que_ent_ptr_t)csa->th_base);
	bt->tn = lcl_tn;
	return bt;
}
