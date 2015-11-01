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

/* includes */
#include "mdef.h"
#include "gtm_stdio.h"

#include "gtm_string.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbml.h"
#include "testpt.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "min_max.h"
#include "send_msg.h"

/* global refs/defs */

/* defines */
#define FAKE_DIRTY	((trans_num)(-1))

bool wcs_verify(gd_region *reg, boolean_t expect_damage)
{
	/* This routine verifies the shared memory structures used to manage the buffers of the bg access method.
	 * Changes to those structures or the way that they are managed may require changes to this routine
	 * some fields may not be rigorously tested if their interrelationships did not seem
	 * important, well defined or well understood, i.e. feel free to make improvements.
	 * It *corrects* errors which have a point nature and
	 * returns a FALSE for systemic problems that require a wcs_recover or something more drastic.
	 */

	uint4			cnt, lcnt, offset, tmp_tn;
	int4			bp, bp_top, cr_base, cr_top, bt_top_off, bt_base_off, i;
	bool			ret;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	cache_rec_ptr_t		cr, cr0, cr1, cr_hi, cr_lo, cr_qbase;
	bt_rec_ptr_t		bt, bt0, bt1, bt_hi, bt_lo;
	th_rec_ptr_t		th, th1;
	cache_que_head_ptr_t	que_head;
	cache_state_rec_ptr_t	cstt, cstt1;
	char			secshr_string[2048];
	char			secshr_string_delta[256];

	error_def(ERR_DBFHEADERR);
	error_def(ERR_DBADDRANGE);
	error_def(ERR_DBQUELINK);
	error_def(ERR_DBCRERR);
	error_def(ERR_DBCLNUPINFO);

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	ret = TRUE;
	/* while some errors terminate loops, as of this writing, no errors are treated as terminal */

	if ((csa->now_crit == FALSE) && (csd->clustered == FALSE))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg), RTS_ERROR_TEXT("now_crit"), csa->now_crit, TRUE);
		grab_crit(reg);		/* what if it has it but loast track of it ??? should there be a crit reset ??? */
	}
	offset = ROUND_UP(sizeof(sgmnt_data), (sizeof(int4) * 2));
	if (csa->nl->bt_header_off != offset)				/* bt_header is "quadword-aligned" after the header */
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("bt_header_off"), csa->nl->bt_header_off, offset);
		csa->nl->bt_header_off = offset;
	}
	if (csa->bt_header != (bt_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->bt_header_off))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("bt_header"), csa->bt_header, (sm_uc_ptr_t)csd + csa->nl->bt_header_off);
		csa->bt_header = (bt_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->bt_header_off);
	}
	offset += csd->bt_buckets * sizeof(bt_rec);
	if (csa->nl->th_base_off != (offset + sizeof(bt->blkque)))	/* th_base follows, skipping the initial blkque heads */
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("th_base_off"), csa->nl->th_base_off, offset + sizeof(bt->blkque));
		csa->nl->th_base_off = (offset + sizeof(bt->blkque));
	}
	if (csa->th_base != (th_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->th_base_off))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("th_base"), csa->th_base, (sm_uc_ptr_t)csd + csa->nl->th_base_off);
		csa->th_base = (th_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->th_base_off);
	}
	offset += sizeof(bt_rec);
	if (csa->nl->bt_base_off != offset)				/* bt_base just skips the item used as the tnque head */
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("bt_base_off"), csa->nl->bt_base_off, offset);
		csa->nl->bt_base_off = offset;
	}
	if (csa->bt_base != (bt_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->bt_base_off))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("bt_base"), csa->bt_base, (sm_uc_ptr_t)csd + csa->nl->bt_base_off);
		csa->bt_base = (bt_rec_ptr_t)((sm_uc_ptr_t)csd + csa->nl->bt_base_off);
	}
	offset += csd->n_bts * sizeof(bt_rec);
	if (0 != (csa->nl->cache_off + CACHE_CONTROL_SIZE(csd)))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("cache_off"), csa->nl->cache_off, -CACHE_CONTROL_SIZE(csd));
		csa->nl->cache_off = -CACHE_CONTROL_SIZE(csd);
	}
	if (csa->acc_meth.bg.cache_state != (cache_que_heads_ptr_t)((sm_uc_ptr_t)csd + csa->nl->cache_off))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("cache_state"), csa->acc_meth.bg.cache_state, (sm_uc_ptr_t)csd + csa->nl->cache_off);
		csa->acc_meth.bg.cache_state = (cache_que_heads_ptr_t)((sm_uc_ptr_t)csd + csa->nl->cache_off);
	}
	if (csd->bt_buckets != getprime(csd->n_bts))
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("bt_buckets"), csd->bt_buckets, getprime(csd->n_bts));
		csd->bt_buckets = getprime(csd->n_bts);
	}
	bt_lo = csa->bt_base;
	bt_hi = bt_lo + csd->n_bts;
	cr_lo = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
	cr_hi = cr_lo + csd->n_bts;
	cr_base = GDS_ANY_ABS2REL(csa, cr_lo);
	cr_top = GDS_ANY_ABS2REL(csa, cr_hi);
	if (csd->wc_blocked == FALSE)
	{ /* in UNIX this blocks the writer */
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("wc_blocked"), csd->wc_blocked, TRUE);
		csd->wc_blocked = TRUE;
	}
	if (csa->nl->in_wtstart != 0)
	{	/* caller should outwait active writers */
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("in_wtstart"), csa->nl->in_wtstart, 0);
		csa->nl->in_wtstart = 0;
	}
	th = csa->th_base;
	if (th->blk != BT_QUEHEAD)
	{
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("th_base->blk"), th->blk, BT_QUEHEAD);
		th->blk = BT_QUEHEAD;
	}

	/* loop through tnque */
	for (th = (th_rec_ptr_t)((sm_uc_ptr_t)th + csa->th_base->tnque.fl), cnt = csd->n_bts, tmp_tn = 0, th1 = th,
		cnt = csd->n_bts + 1; (th1 != csa->th_base) && (cnt > 0); th = th1, cnt--)
	{
		if (((bt_rec_ptr_t)th < bt_lo) || ((bt_rec_ptr_t)th >= bt_hi))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg), th, th, RTS_ERROR_TEXT("tnque"), bt_lo, bt_hi);
			break;
		}
		th1 = (th_rec_ptr_t)((sm_uc_ptr_t)th + th->tnque.fl);
		if ((th_rec_ptr_t)((sm_uc_ptr_t)th1 + th1->tnque.bl) != th)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				th1, th1->blk, RTS_ERROR_TEXT("tnque"), th1->tnque.bl, th - th1);
			break;
		}
		if (th->tn != 0)
		{
			if (th->tn < tmp_tn)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					th, tmp_tn, RTS_ERROR_TEXT("tnque transaction number"), 1, th->tn);
			}
			tmp_tn = th->tn;
		}
		if (((int)(th->blk) != BT_NOTVALID) && (((int)(th->blk) < 0) || ((int)(th->blk) > csa->ti->total_blks)))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				th, th->blk, RTS_ERROR_TEXT("th->blk"), 0, csa->ti->total_blks);
		}
		if (((int)(th->cache_index) != CR_NOTVALID) &&
			(((int)(th->cache_index) < cr_base) || ((int)(th->cache_index) >= cr_top)))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				th, th->cache_index, RTS_ERROR_TEXT("th->cache_index"), cr_base, cr_top);
		}
		if (th->flushing != FALSE)	/* ??? this is a gt.cx item that may require more synchronization at the top */
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
				RTS_ERROR_TEXT("th->flushing"), th->flushing, FALSE);
		}
	}
	if (cnt != 1)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("tnque entries"), csd->n_bts - cnt, csd->n_bts - 1);
	}
	if (tmp_tn > csd->trans_hist.curr_tn)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("th_base->tn"), tmp_tn, csd->trans_hist.curr_tn);
	}

	/* loop through bt blkques */
	for (bt0 = csa->bt_header; bt0 < bt_lo; bt0++)
	{
		if (bt0->blk != BT_QUEHEAD)
		{
			send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
				RTS_ERROR_TEXT("queue head bt->blk"), bt0->blk, BT_QUEHEAD);
			bt0->blk = BT_QUEHEAD;
		}

		for (bt = (bt_rec_ptr_t)((sm_uc_ptr_t)bt0 + bt0->blkque.fl), bt1 = bt, cnt = csd->n_bts + 1;
			(bt1 != bt0) && (cnt > 0);
			bt = bt1, cnt--)
		{
			if ((bt < bt_lo) || (bt >= bt_hi))
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					bt, bt, RTS_ERROR_TEXT("btque"), bt_lo, bt_hi);
				break;
			}
			bt1 = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.fl);
			if ((bt_rec_ptr_t)((sm_uc_ptr_t)bt1 + bt1->blkque.bl) != bt)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
					bt1, bt1->blk, RTS_ERROR_TEXT("btque"), bt1->blkque.bl, (bt - bt1) * sizeof(bt_rec));
				break;
			}
			if ((int)(bt->blk) != BT_NOTVALID)
			{
				if ((csa->bt_header + (bt->blk % csd->bt_buckets)) != bt0)
				{
					assert(expect_damage);
					ret = FALSE;
					send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
						RTS_ERROR_TEXT("bt hash"), bt0 - csa->bt_header, bt->blk % csd->bt_buckets);
				}
				if (CR_NOTVALID != bt->cache_index)
				{
					cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
					if (cr->blk != bt->blk)
					{
						assert(expect_damage);
						ret = FALSE;
						send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg), cr,
							CR_BLKEMPTY != cr->blk ? cr->blk : bt->blk, RTS_ERROR_TEXT("block"),
							cr->blk, bt->blk);
					}
				}
			}
		}
		if (cnt == 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(8) ERR_DBFHEADERR, 6, DB_LEN_STR(reg),
				RTS_ERROR_TEXT("btque entries"), csd->n_bts + 1 - cnt, csd->n_bts + 1);
		}
	}

	bp = ROUND_UP(cr_top, DISK_BLOCK_SIZE);
	bp_top = bp + (csd->n_bts * csd->blk_size);
	bt_base_off = GDS_ANY_ABS2REL(csa, (sm_uc_ptr_t)csd + csa->nl->bt_base_off);
	bt_top_off = GDS_ANY_ABS2REL(csa, (sm_uc_ptr_t)csd + offset);

	/* print info. that secshr_db_clnup stored */
	if (0 != csd->secshr_ops_index)
	{
		if (sizeof(csd->secshr_ops_array) < csd->secshr_ops_index)
		{
			SPRINTF(secshr_string, "secshr_max_index exceeded. max_index = %d [0x%08x] : ops_index = %d [0x%08x]",
					sizeof(csd->secshr_ops_array), sizeof(csd->secshr_ops_array),
					csd->secshr_ops_index, csd->secshr_ops_index);
			send_msg(VARLSTCNT(6) ERR_DBCLNUPINFO, 4, DB_LEN_STR(reg), RTS_ERROR_TEXT(secshr_string));
			csd->secshr_ops_index = sizeof(csd->secshr_ops_array);
		}
		for (i = 0; (i + 1) < csd->secshr_ops_index; i += csd->secshr_ops_array[i])
		{
			SPRINTF(secshr_string, "Line %3d ", csd->secshr_ops_array[i + 1]);
			for (lcnt = i + 2; lcnt < MIN(csd->secshr_ops_index, i + csd->secshr_ops_array[i]); lcnt++)
			{
				SPRINTF(secshr_string_delta, " : [0x%08x]", csd->secshr_ops_array[lcnt]);
				strcat(secshr_string, secshr_string_delta);
			}
			send_msg(VARLSTCNT(6) ERR_DBCLNUPINFO, 4, DB_LEN_STR(reg), RTS_ERROR_TEXT(secshr_string));
		}
		csd->secshr_ops_index = 0;
	}

	/* loop through the cache_recs */
	for (cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets, cnt = csd->n_bts; cnt > 0; cr++, cnt--)
	{
		if (((int)(cr->blk) != CR_BLKEMPTY) && (((int)(cr->blk) < 0) || ((int)(cr->blk) >= csa->ti->total_blks)))
		{
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->blk"), 0, csa->ti->total_blks);
			cr->blk = CR_BLKEMPTY;
		}
		if (cr->tn > (csd->trans_hist.curr_tn + 1))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				cr, cr->tn, RTS_ERROR_TEXT("cr->tn"), 0, csd->trans_hist.curr_tn + 1);
		}
		if ((int)(cr->bt_index) != 0)
		{
			if (((int)(cr->bt_index) < bt_base_off) || ((int)(cr->bt_index) >= bt_top_off))
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					cr, cr->bt_index, RTS_ERROR_TEXT("cr->bt_index"), csa->nl->bt_base_off, offset);
			}
			bt = (bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->bt_index);
			if (cr->blk != bt->blk)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, CR_BLKEMPTY != cr->blk ? cr->blk : bt->blk, RTS_ERROR_TEXT("block"), cr->blk, bt->blk);
			}
		}
		if ((cr->buffaddr < bp) || (cr->buffaddr >= bp_top))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				cr, cr->buffaddr, RTS_ERROR_TEXT("cr->buffaddr"), bp, bp_top);
		}
		if (((int)cr->blk != CR_BLKEMPTY) && (cr->data_invalid != FALSE) && (cr->in_tend != TRUE))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->data_invalid"), cr->data_invalid, FALSE);
		}
		if ((cr->r_epid != 0) && (cr->read_in_progress < 0))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->r_epid"), cr->r_epid, 0);
		}
		if ((cr->in_cw_set != TRUE) && (cr->in_cw_set != FALSE))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				cr, cr->in_cw_set, RTS_ERROR_TEXT("cr->in_cw_set"), FALSE, TRUE);
		}
		if (csa->jnl != NULL)
		{
			if ((cr->jnl_addr > csa->jnl->jnl_buff->freeaddr) && (0 != cr->dirty))
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					cr, (uint4)cr->jnl_addr, RTS_ERROR_TEXT("cr->jnl_addr"), 0, csa->jnl->jnl_buff->freeaddr);
			}
		} else
		{
			if (cr->jnl_addr != 0)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->jnl_addr"), (uint4)cr->jnl_addr, 0);
			}
		}
		if ((WRITE_LATCH_VAL(cr) < LATCH_CLEAR) || (WRITE_LATCH_VAL(cr) > LATCH_CONFLICT))
		{	/* the message would read cr->interlock.semaphore although in Unix it means cr->interlock.latch */
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				cr, WRITE_LATCH_VAL(cr), RTS_ERROR_TEXT("cr->interlock.semaphore"), LATCH_CLEAR, LATCH_CONFLICT);
		}
		/* as of this time cycle is believed to be a relative timestamp with no characteristics useful to verify */
#ifdef VMS
		if (cr->rip_latch.latch_pid != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->rip_latch"), cr->rip_latch.latch_pid, 0);
		}
		if (cr->iosb[0] != 0)
		{
			if (0 == cr->dirty)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("cr->cr->dirty"), cr->dirty, TRUE);
			}
			if (cr->epid == 0)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("cr->epid"), cr->epid, -1);
			}
		}
		if ((WRT_STRT_PNDNG == cr->iosb[0]) && (0 == cr->dirty))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->dirty"), cr->dirty, TRUE);
		}
		if (cr->twin != 0)
		{
			cr1 = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
			if (cr != (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr1->twin))
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("cr->twin->twin"), cr, GDS_ANY_REL2ABS(csa, cr1->twin));
			}
		}
		if (cr->stopped == TRUE)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->stopped"), cr->stopped, FALSE);
		} else
		{
			if (cr->stopped != FALSE)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					cr, cr->stopped, RTS_ERROR_TEXT("cr->stopped"), FALSE, TRUE);
			}
		}
		if (cr->wip_stopped == TRUE)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->wip_stopped"), cr->wip_stopped, FALSE);
		} else
		{
			if (cr->wip_stopped != FALSE)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					cr, cr->wip_stopped, RTS_ERROR_TEXT("cr->wip_stopped"), FALSE, TRUE);
			}
		}
#else
		/* as of this time image_count is believed to be a relative timestamp with no characteristics useful to verify */
		/* all of the following fields are currently unused outside of VMS */
		for (i=0; i < 4; i++)
		{
			if (cr->iosb[i] != 0)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("cr->iosb"), cr->iosb[i], 0);
			}
		}
		if (cr->twin != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->twin"), cr->twin, 0);
		}
		if (cr->epid != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->epid"), cr->epid, 0);
		}
		if (cr->image_count != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->image_count"), cr->image_count, 0);
		}
		if (cr->stopped != FALSE)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->stopped"), cr->stopped, 0);
		}
		if (cr->wip_stopped != FALSE)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("cr->wip_stopped"), cr->wip_stopped, 0);
		}
#endif
	}

	/* loop through the cr blkques */
	for (cr0 = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array, cr_qbase = cr0; cr0 < cr_lo; cr0++)
	{
		if (cr0->blk != BT_QUEHEAD)
		{
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				cr, cr->blk, RTS_ERROR_TEXT("queue head cr->blk"), cr0->blk, BT_QUEHEAD);
			cr0->blk = BT_QUEHEAD;
		}
		for (cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr0 + cr0->blkque.fl), cr1 = cr, cnt = csd->n_bts + 1;
			(cr1 != cr0) && (cnt > 0);
			cr = cr1, cnt--)
		{
			if ((cr < cr_lo) || (cr >= cr_hi))
			{
				assert(expect_damage);
				ret=FALSE;
				send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
					cr, cr, RTS_ERROR_TEXT("crque"), cr_lo, cr_hi);
				break;
			}
			cr1 = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.fl);
			if ((cache_rec_ptr_t)((sm_uc_ptr_t)cr1 + cr1->blkque.bl) != cr)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
					cr1, cr1->blk, RTS_ERROR_TEXT("crque"), cr1->blkque.bl, (cr - cr1) * sizeof(cache_rec));
				break;
			}
			if (((int)(cr->blk) != CR_NOTVALID) && ((cr_qbase + (cr->blk % csd->bt_buckets)) != cr0))
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("cr hash"), cr0 - cr_qbase, cr->blk % csd->bt_buckets);
			}
		}
		if (cnt == 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				cr_qbase, 0, RTS_ERROR_TEXT("crque entries"), csd->n_bts + 1, csd->n_bts);
		}
	}
	que_head = &csa->acc_meth.bg.cache_state->cacheq_active;
	if ((sm_long_t)que_head % sizeof(que_ent) != 0)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), que_head, 0, RTS_ERROR_TEXT("cacheq_active"), que_head,
			((sm_long_t)que_head / (int4)sizeof(que_ent)) * (int4)sizeof(que_ent));
	}
	/* loop through the active queue */
	for (cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)que_head + que_head->fl), cnt = csd->n_bts;
		(cstt !=(cache_state_rec_ptr_t)que_head) && (cnt > 0);
		cstt = cstt1, cnt--)
	{
		if (((cache_rec_ptr_t)cstt < cr_lo) || ((cache_rec_ptr_t)cstt >= cr_hi))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				(int)cstt + sizeof(que_head), cstt, RTS_ERROR_TEXT("active queue"), cr_lo, cr_hi);
			break;
		}
		cstt1 = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.fl);
		if ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt1 + cstt1->state_que.bl) != cstt)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), (int)cstt1 + sizeof(que_head), 0,
				RTS_ERROR_TEXT("active queue"), cstt1->state_que.bl, (cstt - cstt1) * sizeof(cache_rec));
			break;
		}
		if (0 == cstt->dirty)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				(int)cstt + sizeof(que_head), cstt->blk, RTS_ERROR_TEXT("active cr->dirty"), cstt->dirty, TRUE);
		}
		if ((cstt->flushed_dirty_tn && (cstt->dirty <= cstt->flushed_dirty_tn))
			 || (cstt->dirty > csd->trans_hist.curr_tn))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg), (int)cstt + sizeof(que_head), cstt->dirty,
				RTS_ERROR_TEXT("dirty (tn)"), cstt->flushed_dirty_tn + 1, csd->trans_hist.curr_tn);
		}
		cstt->dirty = FAKE_DIRTY;	/* change the flag to indicate it was found in a state queue */
	}
	if (cnt == 0)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
			que_head, 0, RTS_ERROR_TEXT("active queue entries"), csd->n_bts + 1, csd->n_bts);
	}

	/* loop through the wip queue */
	que_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
	if ((sm_long_t)que_head % sizeof(que_ent) != 0)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), que_head, 0, RTS_ERROR_TEXT("cacheq_wip"), que_head,
			((sm_long_t)que_head / (int4)sizeof(que_ent)) * (int4)sizeof(que_ent));
	}
#ifdef VMS
	for (cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)que_head + que_head->fl), cnt = csd->n_bts;
		(cstt !=(cache_state_rec_ptr_t)que_head) && (cnt > 0);
		cstt = cstt1, cnt--)
	{
		if (((cache_rec_ptr_t)cstt < cr_lo) || ((cache_rec_ptr_t)cstt >= cr_hi))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg),
				(int)cstt + sizeof(que_head), cstt, RTS_ERROR_TEXT("wip queue"), cr_lo, cr_hi);
			break;
		}
		cstt1 = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.fl);
		if ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt1 + cstt1->state_que.bl) != cstt)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), (int)cstt1 + sizeof(que_head), 0,
				RTS_ERROR_TEXT("wip queue"), cstt1->state_que.bl, (cstt - cstt1) * sizeof(cache_rec));
			break;
		}
/*	secondary failure @ ipb - not yet determined if it was a legal state or a recover problem
		if (cstt->epid == 0)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				(int)cstt + sizeof(state_que), cstt->blk, RTS_ERROR_TEXT("wip cr->epid"), cstt->epid, -1);
		}
*/
		if (0 == cstt->dirty)
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
				(int)cstt + sizeof(que_head), cstt->blk, RTS_ERROR_TEXT("wip cr->dirty"), cstt->dirty, TRUE);
		}
		if (((0 != cstt->flushed_dirty_tn) && (cstt->dirty <= cstt->flushed_dirty_tn))
			 || (cstt->dirty > csd->trans_hist.curr_tn))
		{
			assert(expect_damage);
			ret = FALSE;
			send_msg(VARLSTCNT(10) ERR_DBADDRANGE, 8, DB_LEN_STR(reg), (int)cstt + sizeof(que_head), cstt->dirty,
				RTS_ERROR_TEXT("dirty (tn)"), cstt->flushed_dirty_tn + 1, csd->trans_hist.curr_tn);
		}
		cstt->dirty = FAKE_DIRTY;	/* change the flag to indicate it was found in a state queue */
	}
	if (cnt == 0)
	{
		assert(expect_damage);
		ret = FALSE;
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
			que_head, 0, RTS_ERROR_TEXT("wip queue entries"), csd->n_bts + 1, csd->n_bts);
	}
#else
	if (que_head->fl != 0)
	{
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
			que_head, 0, RTS_ERROR_TEXT("wip queue head fl"), que_head->fl, 0);
		que_head->fl = 0;
	}
	if (que_head->bl != 0)
	{
		send_msg(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
			que_head, 0, RTS_ERROR_TEXT("wip queue head bl"), que_head->bl, 0);
		que_head->bl = 0;
	}
#endif

	/* loop through the cache_recs again to look for lost dirties */
	for (cr = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets, cnt = csd->n_bts; cnt > 0; cr++, cnt--)
	{
		if (cr->dirty == FAKE_DIRTY)
			cr->dirty = cr->flushed_dirty_tn + 1;
		else
		{
			if (0 != cr->dirty)
			{
				assert(expect_damage);
				ret = FALSE;
				send_msg(VARLSTCNT(10) ERR_DBCRERR, 8, DB_LEN_STR(reg),
					cr, cr->blk, RTS_ERROR_TEXT("non-state cr->dirty"), cr->dirty, FALSE);
			}
		}
	}
	return ret;
}
