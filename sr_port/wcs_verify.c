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
#include "cert_blk.h"
#include "memcoherency.h"

/* global refs/defs */

/* defines */
#define FAKE_DIRTY		((trans_num)(-1))
#ifdef UNIX
#define SEND_MSG_CSA(...)	send_msg_csa(CSA_ARG(csa) __VA_ARGS__)	/* to avoid formatting various send_msg calls */
#else
#define SEND_MSG_CSA		send_msg
#endif

GBLREF 	uint4			process_id;

error_def(ERR_DBADDRALIGN);
error_def(ERR_DBADDRANGE);
error_def(ERR_DBADDRANGE8);
error_def(ERR_DBCLNUPINFO);
error_def(ERR_DBCRERR);
error_def(ERR_DBCRERR8);
error_def(ERR_DBFHEADERR4);
error_def(ERR_DBFHEADERR8);
error_def(ERR_DBFHEADERRANY);
error_def(ERR_DBQUELINK);
error_def(ERR_DBWCVERIFYEND);
error_def(ERR_DBWCVERIFYSTART);

boolean_t	wcs_verify(gd_region *reg, boolean_t expect_damage, boolean_t caller_is_wcs_recover)
{	/* This routine verifies the shared memory structures used to manage the buffers of the bg access method.
	 * Changes to those structures or the way that they are managed may require changes to this routine
	 * some fields may not be rigorously tested if their interrelationships did not seem
	 * important, well defined or well understood, i.e. feel free to make improvements.
	 * It *corrects* errors which have a point nature and
	 * returns a FALSE for systemic problems that require a wcs_recover or something more drastic.
	 */

	uint4			cnt, lcnt ;
        ssize_t			offset ;
        trans_num 		max_tn, tmp_8byte;
	INTPTR_T		bp_lo, bp_top, bp, cr_base, cr_top, bt_top_off, bt_base_off;
	sm_uc_ptr_t		bptmp;
	boolean_t		ret;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	cache_rec_ptr_t		cr, cr0, cr_tmp, cr_prev, cr_hi, cr_lo, cr_qbase;
	bt_rec_ptr_t		bt, bt0, bt_prev, bt_hi, bt_lo;
	th_rec_ptr_t		th, th_prev;
	cache_que_head_ptr_t	que_head;
	cache_state_rec_ptr_t	cstt, cstt_prev;
	char			secshr_string[2048];
	char			secshr_string_delta[256];
	sm_uc_ptr_t		jnl_buff_expected;
	boolean_t		(*blkque_array)[] = NULL; /* TRUE indicates we saw the cr or bt of that array index */
	int4			i, n_bts;	/* a copy of csd->n_bts since it is used frequently in this routine */
	trans_num		dummy_tn;
	int4			in_wtstart, intent_wtstart, wcs_phase2_commit_pidcnt;

	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	ret = TRUE;
	SEND_MSG_CSA(VARLSTCNT(7) ERR_DBWCVERIFYSTART, 5, DB_LEN_STR(reg), process_id, process_id, &csd->trans_hist.curr_tn);
	/* while some errors terminate loops, as of this writing, no errors are treated as terminal */
	if ((csa->now_crit == FALSE) && (csd->clustered == FALSE))
	{
		assert(expect_damage);
		assert(!csa->hold_onto_crit);
		ret = FALSE;
		SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg), RTS_ERROR_TEXT("now_crit"), csa->now_crit, TRUE);
		grab_crit(reg);		/* what if it has it but lost track of it ??? should there be a crit reset ??? */
	}
	if (dba_mm != csd->acc_meth)
	{
		offset = ROUND_UP(SIZEOF_FILE_HDR(csd), (SIZEOF(int4) * 2));
		if (cnl->bt_header_off != offset)			/* bt_header is "quadword-aligned" after the header */
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("bt_header_off"), cnl->bt_header_off, offset);
			cnl->bt_header_off = offset;
		}
		if (csa->bt_header != (bt_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->bt_header_off))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("bt_header"), csa->bt_header, (sm_uc_ptr_t)csd + cnl->bt_header_off);
			csa->bt_header = (bt_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->bt_header_off);
		}
		offset += csd->bt_buckets * SIZEOF(bt_rec);
		if (cnl->th_base_off != (offset + SIZEOF(bt->blkque)))	/* th_base follows, skipping the initial blkque heads */
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("th_base_off"), cnl->th_base_off, offset + SIZEOF(bt->blkque));
			cnl->th_base_off = (offset + SIZEOF(bt->blkque));
		}
		if (csa->th_base != (th_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->th_base_off))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("th_base"), csa->th_base, (sm_uc_ptr_t)csd + cnl->th_base_off);
			csa->th_base = (th_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->th_base_off);
		}
		offset += SIZEOF(bt_rec);
		if (cnl->bt_base_off != offset)				/* bt_base just skips the item used as the tnque head */
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("bt_base_off"), cnl->bt_base_off, offset);
			cnl->bt_base_off = offset;
		}
		if (csa->bt_base != (bt_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->bt_base_off))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("bt_base"), csa->bt_base, (sm_uc_ptr_t)csd + cnl->bt_base_off);
			csa->bt_base = (bt_rec_ptr_t)((sm_uc_ptr_t)csd + cnl->bt_base_off);
		}
	} else
		n_bts = csd->n_bts;
	if (csa->ti != &csd->trans_hist)
	{
		assert(expect_damage);
		ret = FALSE;
		SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
			RTS_ERROR_TEXT("csa->ti"), (sm_uc_ptr_t)csa->ti, (sm_uc_ptr_t)&csd->trans_hist);
		csa->ti = &csd->trans_hist;
	}
	if (dba_mm != csd->acc_meth)
	{
		n_bts = csd->n_bts;
		offset += n_bts * SIZEOF(bt_rec);
		if (0 != (cnl->cache_off + CACHE_CONTROL_SIZE(csd)))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("cache_off"), cnl->cache_off, -CACHE_CONTROL_SIZE(csd));
			cnl->cache_off = -CACHE_CONTROL_SIZE(csd);
		}
		if (csa->acc_meth.bg.cache_state != (cache_que_heads_ptr_t)((sm_uc_ptr_t)csd + cnl->cache_off))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("cache_state"), csa->acc_meth.bg.cache_state, (sm_uc_ptr_t)csd + cnl->cache_off);
			csa->acc_meth.bg.cache_state = (cache_que_heads_ptr_t)((sm_uc_ptr_t)csd + cnl->cache_off);
		}
		if (csd->bt_buckets != getprime(n_bts))
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("bt_buckets"), csd->bt_buckets, getprime(n_bts));
			csd->bt_buckets = getprime(n_bts);
		}
	}
	if (JNL_ALLOWED(csd))
	{
		if (NULL == csa->jnl)
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
				RTS_ERROR_TEXT("csa->jnl"), csa->jnl, (UINTPTR_T)-1);
		else if (NULL == csa->jnl->jnl_buff)
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
					RTS_ERROR_TEXT("csa->jnl->jnl_buff"), csa->jnl->jnl_buff, (UINTPTR_T)-1);
		else
		{
			jnl_buff_expected = ((sm_uc_ptr_t)(cnl) + NODE_LOCAL_SPACE(csd) + JNL_NAME_EXP_SIZE);
			if (csa->jnl->jnl_buff != (jnl_buffer_ptr_t)jnl_buff_expected)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERRANY, 6, DB_LEN_STR(reg),
					RTS_ERROR_TEXT("csa->jnl->jnl_buff_expected"), csa->jnl->jnl_buff, jnl_buff_expected);
				csa->jnl->jnl_buff = (jnl_buffer_ptr_t)jnl_buff_expected;
			}
		}
	}
	if (dba_mm != csd->acc_meth)
	{
		bt_lo = csa->bt_base;
		bt_hi = bt_lo + n_bts;
		cr_lo = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets;
		cr_hi = cr_lo + n_bts;
		cr_base = GDS_ANY_ABS2REL(csa, cr_lo);
		cr_top = GDS_ANY_ABS2REL(csa, cr_hi);
		if (caller_is_wcs_recover)
		{	/* if wcs_recover is caller, it would have waited for the following fields to become 0.
			 * if called from DSE CACHE -VERIFY, none of these are guaranteed. So do these checks only for first case.
			 */
			if (FALSE == cnl->wc_blocked)
			{	/* in UNIX this blocks the writer */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("wc_blocked"), cnl->wc_blocked, TRUE);
				SET_TRACEABLE_VAR(cnl->wc_blocked, TRUE);
			}
			in_wtstart = cnl->in_wtstart;	/* store value in local variable in case the following assert fails */
			if (0 != in_wtstart)
			{	/* caller should outwait active writers */
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg), RTS_ERROR_TEXT("in_wtstart"),
					 in_wtstart, 0);
				assert(expect_damage);
				cnl->in_wtstart = 0;
				csa->in_wtstart = FALSE; /* To allow wcs_wtstart() after wcs_recover() */
			}
			intent_wtstart = cnl->intent_wtstart;
			if (0 != intent_wtstart)
			{	/* Two situations are possible.
				 *	a) A wcs_wtstart() call is concurrently in progress and that the process has just now
				 *		incremented intent_wtstart. It will notice cnl->wc_blocked to be TRUE and
				 *		decrement intent_wtstart right away and return. So we dont need to do anything.
				 *	b) A wcs_wtstart() call had previously increment intent_wtstart but got shot before it could
				 *		get a chance to decrement the field. In this case, we need to clear the field to
				 *		recover from this situation.
				 *	Since the writer uses the DECR_INTENT_WTSTART macro which does not do DECR_CNTs if the value
				 *	is already 0, it is okay to do the decrement even in case (a). There is a very small window
				 *	that still exists. If the DECR_INTENT_WTSTART macro did the > 0 check when the field was
				 *	positive and then wcs_verify reset the field, the DECR_CNT will happen and will cause it to
				 *	become negative. But that will last until the next INCR_INTENT_WTSTART or wcs_recover which
				 *	happens first. The INCR_INTENT_WTSTART macro has a double increment to take care of this
				 *	case.
				 */
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("intent_wtstart"), intent_wtstart, 0);
				cnl->intent_wtstart = 0;
				SHM_WRITE_MEMORY_BARRIER;
			}
			wcs_phase2_commit_pidcnt = cnl->wcs_phase2_commit_pidcnt; /* store value in local in case assert fails */
			if (0 != wcs_phase2_commit_pidcnt)
			{	/* caller should outwait active committers */
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("wcs_phase2_commit_pidcnt"), wcs_phase2_commit_pidcnt, 0);
				assert(expect_damage);
				cnl->wcs_phase2_commit_pidcnt = 0;
				csa->wcs_pidcnt_incremented = FALSE; /* Just to be safe */
			}
		}
		th = csa->th_base;
		if (th->blk != BT_QUEHEAD)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("th_base->blk"), th->blk, BT_QUEHEAD);
			th->blk = BT_QUEHEAD;
		}
		/* loop through bt tnque */
		for (th_prev = th, th = (th_rec_ptr_t)((sm_uc_ptr_t)th + th->tnque.fl), cnt = n_bts, max_tn = 0, cnt = n_bts + 1;
		     (th != csa->th_base) && (cnt > 0);
		     th_prev = th, cnt--, th = (th_rec_ptr_t)((sm_uc_ptr_t)th + th->tnque.fl))
		{
			bt = (bt_rec_ptr_t)((sm_uc_ptr_t)th - SIZEOF(bt->blkque));
			if (BT_NOT_ALIGNED(bt, bt_lo))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), th_prev, -1,
					 RTS_ERROR_TEXT("th->tnque"), bt, bt_lo, SIZEOF(bt_rec));
				break;
			}
			if (BT_NOT_IN_RANGE(bt, bt_lo, bt_hi))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), th_prev, -1,
					 bt, RTS_ERROR_TEXT("th->tnque"), bt_lo, bt_hi);
				break;
			}
			if ((th_rec_ptr_t)((sm_uc_ptr_t)th + th->tnque.bl) != th_prev)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), th, th->blk,
						RTS_ERROR_TEXT("tnque.bl"), (UINTPTR_T)th->tnque.bl,
						(sm_uc_ptr_t)th_prev - (sm_uc_ptr_t)th);
			}
			if (th->tn != 0)
			{
				if (th->tn < max_tn)
				{
					assert(expect_damage);
					ret = FALSE;
					tmp_8byte = 1;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE8, 9, DB_LEN_STR(reg), th, th->blk, &max_tn,
							RTS_ERROR_TEXT("tnque transaction number"), &tmp_8byte, &th->tn);
				}
				/* ideally, the following max_tn assignment should have been in the else part of the above if. but
				 *  the issue with doing that is if there is a sequence of non-decreasing transaction numbers
				 * except for one (or few) numbers in the middle of the sequence that is larger than all others,
				 * it is more likely that those hiccups are incorrect. in that case we do not want max_tn to end
				 * up being an incorrect large value. hence the unconditional assignment below.
				 */
				max_tn = th->tn;
			}
			if (((int)(th->blk) != BT_NOTVALID) &&
			    (((int)(th->blk) < 0) || ((int)(th->blk) > csd->trans_hist.total_blks)))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
					 th, th->blk, th->blk, RTS_ERROR_TEXT("th->blk"), 0, csd->trans_hist.total_blks);
			}
			if (((int)(th->cache_index) != CR_NOTVALID) &&
			    (((int)(th->cache_index) < cr_base) || ((int)(th->cache_index) >= cr_top)))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
					 th, th->blk, th->cache_index, RTS_ERROR_TEXT("th->cache_index"), cr_base, cr_top);
			}
			if (th->flushing != FALSE) /* ??? this is a gt.cx item that may require more synchronization at the top */
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("th->flushing"), th->flushing, FALSE);
			}
			if (0 == th->tnque.fl)
			{	/* No point proceeding to next iteration of loop as "th + th->tnque.fl" will be the same as "th" */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
					 th, th->blk, RTS_ERROR_TEXT("tnque.fl"), (UINTPTR_T)th->tnque.fl, (UINTPTR_T)-1);
				break;
			}
		}
		if (cnt != 1)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("tnque entries"), n_bts - cnt, n_bts - 1);
		} else if ((th == csa->th_base) && ((th_rec_ptr_t)((sm_uc_ptr_t)th + th->tnque.bl) != th_prev))
		{	/* at this point "th" is csa->th_base and its backlink does not point to the last entry in the th queue */
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), th, th->blk,
				 RTS_ERROR_TEXT("tnque th_base"), (UINTPTR_T)th->tnque.bl, (sm_uc_ptr_t)th_prev - (sm_uc_ptr_t)th);
		}
		if (max_tn > csd->trans_hist.curr_tn)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR8, 6, DB_LEN_STR(reg),
				 RTS_ERROR_TEXT("MAX(th_base->tn)"), &max_tn, &csd->trans_hist.curr_tn);
		}
		/* loop through bt blkques */
		blkque_array = malloc(n_bts * SIZEOF(boolean_t));
		memset(blkque_array, 0, n_bts * SIZEOF(boolean_t));	/* initially, we did not find any bt in the bt blkques */
		for (bt0 = csa->bt_header; bt0 < bt_lo; bt0++)
		{
			if (bt0->blk != BT_QUEHEAD)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("queue head bt->blk"), bt0->blk, BT_QUEHEAD);
				bt0->blk = BT_QUEHEAD;
			}
			for (bt_prev = bt0, bt = (bt_rec_ptr_t)((sm_uc_ptr_t)bt0 + bt0->blkque.fl), cnt = n_bts + 1;
			     (bt != bt0) && (cnt > 0);
			     bt_prev = bt, cnt--, bt = (bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.fl))
			{
				if (BT_NOT_ALIGNED(bt, bt_lo))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), bt_prev, -1,
						 RTS_ERROR_TEXT("bt->blkque"), bt, bt_lo, SIZEOF(bt_rec));
					break;
				}
				if (BT_NOT_IN_RANGE(bt, bt_lo, bt_hi))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), bt_prev, -1,
						 bt, RTS_ERROR_TEXT("bt->blkque"), bt_lo, bt_hi);
					break;
				}
				if ((bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.bl) != bt_prev)
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
						 RTS_ERROR_TEXT("bt->blkque.bl"), (UINTPTR_T)bt->blkque.bl,
						 (sm_uc_ptr_t)bt_prev - (sm_uc_ptr_t)bt);
				}
				if ((int)(bt->blk) != BT_NOTVALID)
				{
					if ((csa->bt_header + (bt->blk % csd->bt_buckets)) != bt0)
					{
						assert(expect_damage);
						ret = FALSE;
						SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
							 RTS_ERROR_TEXT("bt hash"), (bt0 - csa->bt_header),
							 (UINTPTR_T)(bt->blk % csd->bt_buckets));
					}
					if (CR_NOTVALID != bt->cache_index)
					{
						cr = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
						/* Before checking if "cr->blk" is the same as "bt->blk", check if "cr" is valid */
						if (CR_NOT_IN_RANGE(cr, cr_lo, cr_hi))
						{
							assert(expect_damage);
							ret = FALSE;
							SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
								 bt, bt->blk, cr, RTS_ERROR_TEXT("bt->cache_index"), cr_lo, cr_hi);
						} else if (CR_NOT_ALIGNED(cr, cr_lo))
						{
							assert(expect_damage);
							ret = FALSE;
							SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), bt, bt->blk,
								 RTS_ERROR_TEXT("bt->cache_index"), cr, cr_lo, SIZEOF(cache_rec));
						} else if (cr->blk != bt->blk)
						{
							assert(expect_damage);
							ret = FALSE;
							SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr,
								 bt->blk, RTS_ERROR_TEXT("bt block"), cr->blk, bt->blk,
								 CALLFROM);
						}
					}
				}
				(*blkque_array)[bt - bt_lo] = TRUE; /* note: this bt's blkque hash validity is already checked */
				if (0 == bt->blkque.fl)
				{	/* No point proceeding to next iteration as "bt + bt->blkque.fl" will be the same as "bt" */
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
						 RTS_ERROR_TEXT("bt->blkque.fl"), (UINTPTR_T)bt->blkque.fl, (UINTPTR_T)-1);
					break;
				}
			}
			if (cnt == 0)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(8) ERR_DBFHEADERR4, 6, DB_LEN_STR(reg),
					 RTS_ERROR_TEXT("btque entries"), n_bts + 1 - cnt, n_bts + 1);
			} else if ((bt == bt0) && ((bt_rec_ptr_t)((sm_uc_ptr_t)bt + bt->blkque.bl) != bt_prev))
			{	/* at this point "bt" is bt0 and its backlink does not point to last entry in the bt0'th queue */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
					 RTS_ERROR_TEXT("btque bt_base"), (UINTPTR_T)bt->blkque.bl,
					 (sm_uc_ptr_t)bt_prev - (sm_uc_ptr_t)bt);
			}
		}
		/* scan all bts looking for valid bt->blks whose bts were not in any blkque */
		for (bt = bt_lo; bt < bt_hi; bt++)
		{
			if ((FALSE == (*blkque_array)[bt - bt_lo]) && ((int)(bt->blk) != BT_NOTVALID))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), bt, bt->blk,
					 RTS_ERROR_TEXT("bt blkque hash"), (UINTPTR_T)-1, (UINTPTR_T)(bt->blk % csd->bt_buckets));
			}
		}

		bp_lo = ROUND_UP(cr_top, OS_PAGE_SIZE);
		bp_top = bp_lo + ((gtm_uint64_t)n_bts * csd->blk_size);
		bt_base_off = GDS_ANY_ABS2REL(csa, (sm_uc_ptr_t)csd + cnl->bt_base_off);
		bt_top_off = GDS_ANY_ABS2REL(csa, (sm_uc_ptr_t)csd + offset);

		/* print info. that secshr_db_clnup stored */
		if (0 != cnl->secshr_ops_index)
		{
			assert(expect_damage);
			if (SECSHR_OPS_ARRAY_SIZE < cnl->secshr_ops_index)
			{
				SPRINTF(secshr_string,
					"secshr_max_index exceeded. max_index = %d [0x%08x] : ops_index = %d [0x%08x]",
					SECSHR_OPS_ARRAY_SIZE, SECSHR_OPS_ARRAY_SIZE,
					cnl->secshr_ops_index, cnl->secshr_ops_index);
				SEND_MSG_CSA(VARLSTCNT(6) ERR_DBCLNUPINFO, 4, DB_LEN_STR(reg), RTS_ERROR_TEXT(secshr_string));
				cnl->secshr_ops_index = SECSHR_OPS_ARRAY_SIZE;
			}
			for (i = 0; (i + 1) < cnl->secshr_ops_index; i += (int4)cnl->secshr_ops_array[i])
			{
				SPRINTF(secshr_string, "Line %3ld ", cnl->secshr_ops_array[i + 1]);
				for (lcnt = i + 2; lcnt < MIN(cnl->secshr_ops_index, i + cnl->secshr_ops_array[i]); lcnt++)
				{
					SPRINTF(secshr_string_delta, " : [0x%08lx]", cnl->secshr_ops_array[lcnt]);
					strcat(secshr_string, secshr_string_delta);
				}
				SEND_MSG_CSA(VARLSTCNT(6) ERR_DBCLNUPINFO, 4, DB_LEN_STR(reg), RTS_ERROR_TEXT(secshr_string));
			}
			cnl->secshr_ops_index = 0;
		}
		/* loop through the cache_recs */
		memset(blkque_array, 0, n_bts * SIZEOF(boolean_t));	/* initially, we did not find any cr in the cr blkques */
		for (bp = bp_lo, cr = cr_lo, cnt = n_bts; cnt > 0; cr++, bp += csd->blk_size, cnt--)
		{
			if (((int)(cr->blk) != CR_BLKEMPTY) &&
			    (((int)(cr->blk) < 0) || ((int)(cr->blk) >= csd->trans_hist.total_blks)))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
					 cr, cr->blk, cr->blk, RTS_ERROR_TEXT("cr->blk"), 0, csd->trans_hist.total_blks);
			}
			if (cr->tn > csd->trans_hist.curr_tn)
			{
				assert(expect_damage);
				ret = FALSE;
				tmp_8byte = 0;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE8, 9, DB_LEN_STR(reg),
					 cr, cr->blk, &cr->tn, RTS_ERROR_TEXT("cr->tn"), &tmp_8byte, &csd->trans_hist.curr_tn);
			}
			if (0 != cr->bt_index)
			{
				if (!IS_PTR_IN_RANGE(cr->bt_index, bt_base_off, bt_top_off))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
						 cr, cr->blk, cr->bt_index, RTS_ERROR_TEXT("cr->bt_index"), bt_base_off,
						 bt_top_off);
				} else if (!IS_PTR_ALIGNED(cr->bt_index, bt_base_off, SIZEOF(bt_rec)))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->bt_index"), cr->bt_index, bt_base_off,
						 SIZEOF(bt_rec));
				} else
				{
					bt = (bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->bt_index);
					if (cr->blk != bt->blk)
					{
						assert(expect_damage);
						ret = FALSE;
						SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr,
							 cr->blk, RTS_ERROR_TEXT("cr block"), cr->blk, bt->blk,
							 CALLFROM);
					}
				}
			}
			if (!IS_PTR_IN_RANGE(cr->buffaddr, bp_lo, bp_top))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
					 cr, cr->blk, cr->buffaddr, RTS_ERROR_TEXT("cr->buffaddr"), bp_lo, bp_top);
			} else if (!IS_PTR_ALIGNED(cr->buffaddr, bp_lo, csd->blk_size))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->buffaddr"), cr->buffaddr, bp_lo, csd->blk_size);
			} else if (cr->buffaddr != bp)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->buffaddr"), cr->buffaddr, bp, CALLFROM);
			}
			if (cr->in_tend)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->in_tend"), cr->in_tend, FALSE, CALLFROM);
			}
			if (cr->data_invalid)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->data_invalid"), cr->data_invalid, FALSE, CALLFROM);
			}
			if (cr->r_epid != 0)
			{
				if (cr->read_in_progress < 0)
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->r_epid"), cr->r_epid, 0, CALLFROM);
				}
			} else if ((-1 == cr->read_in_progress) && !caller_is_wcs_recover && (CR_BLKEMPTY != cr->blk)
				&& !cr->data_invalid
				VMS_ONLY(&& (!IS_BITMAP_BLK(cr->blk) || (0 == cr->twin) || (0 != cr->bt_index))))
			{	/* If the buffer is not being read into currently (checked both by cr->r_epid being 0 and
				 * cr->read_in_progress being -1) and we are being called from DSE CACHE -VERIFY and cr points
				 * to a valid non-empty block, check the content of cr->buffaddr through a cert_blk().
				 * In VMS, if it is a bitmap block, we could have twins so do check only on newtest twin as
				 * older twin could have an incorrect masterbitmap full/free status (DBBMMSTR error).
				 * Use "bp" as the buffer as cr->buffaddr might be detected as corrupt by the buffaddr checks
				 * above. The reason why the cert_blk() is done only from a DSE CACHE -VERIFY call and not from a
				 * wcs_recover() call is that wcs_recover() is supposed to check the integrity of the data
				 * structures in the cache and not the integrity of the data (global buffers) in the cache. If the
				 * database has an integrity error, a global buffer will fail cert_blk() but the cache structures
				 * as such are not damaged. wcs_recover() should not return failure in that case.
				 */
				bptmp = (sm_uc_ptr_t)GDS_ANY_REL2ABS(csa, bp);
				if (!cert_blk(reg, cr->blk, (blk_hdr_ptr_t)bptmp, 0, FALSE))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("Block certification result"),
						 FALSE, TRUE, CALLFROM);
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
						 RTS_ERROR_TEXT("Block certification result buffer"),
						 bptmp, csa->lock_addrs[0], CALLFROM);
				}
			}
			if (0 != cr->in_cw_set)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->in_cw_set"), (uint4)cr->in_cw_set, 0, CALLFROM);
			}
			assert(!JNL_ALLOWED(csd) || (NULL != csa->jnl) && (NULL != csa->jnl->jnl_buff));
			if (JNL_ENABLED(csd))
			{
				if ((NULL != csa->jnl) && (NULL != csa->jnl->jnl_buff)
				    && (0 != cr->dirty) && (cr->jnl_addr > csa->jnl->jnl_buff->freeaddr))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), cr, cr->blk,
						 (uint4)cr->jnl_addr, RTS_ERROR_TEXT("cr->jnl_addr"), 0,
						 csa->jnl->jnl_buff->freeaddr);
				}
			} else if (!JNL_ALLOWED(csd) && (cr->jnl_addr != 0))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->jnl_addr"), (uint4)cr->jnl_addr, 0, CALLFROM);
			}
			if ((WRITE_LATCH_VAL(cr) < LATCH_CLEAR) || (WRITE_LATCH_VAL(cr) > LATCH_CONFLICT))
			{	/* the message would read cr->interlock.semaphore although in Unix it means cr->interlock.latch */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), cr, cr->blk,
					 WRITE_LATCH_VAL(cr), RTS_ERROR_TEXT("cr->interlock.semaphore"), LATCH_CLEAR,
					 LATCH_CONFLICT);
			}
			/* as of this time cycle is believed to be a relative timestamp with no characteristics useful to verify */
#ifdef VMS
			if (cr->rip_latch.u.parts.latch_pid != 0)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("cr->rip_latch"), cr->rip_latch.u.parts.latch_pid, 0, CALLFROM);
			}
			if (cr->iosb.cond != 0)
			{	/* do not set "ret" to FALSE in both cases below. this is because it seems like VMS can set
				 * iosb.cond to the qio status much after a process that issued the qio died. our current
				 * suspicion is that this occurs because the iosb is in shared memory which is available even
				 * after the process dies. although the two cases below are unexpected, wcs_wtstart()/wcs_wtfini()
				 * handle this well enough that we do not see any need to consider this as a damaged cache. see
				 * D9B11-001992 for details. -- nars - July 2003.
				 */
				if (0 == cr->dirty)
				{
					assert(expect_damage);
					dummy_tn = (trans_num)TRUE;
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR8, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->cr->dirty"), &cr->dirty, &dummy_tn, CALLFROM);
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->cr->iosb"), cr->iosb.cond, 0, CALLFROM);
				}
				if (0 == cr->epid)
				{
					assert(expect_damage);
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->epid"), cr->epid, -1, CALLFROM);
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->iosb"), cr->iosb.cond, 0, CALLFROM);
				}
			}
			if ((WRT_STRT_PNDNG == cr->iosb.cond) && (0 == cr->dirty))
			{
				assert(expect_damage);
				ret = FALSE;
				dummy_tn = (trans_num)TRUE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR8, 11, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("cr->dirty"), &cr->dirty, &dummy_tn, CALLFROM);
			}
			if (cr->twin != 0)
			{
				cr_tmp = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
				if (CR_NOT_IN_RANGE(cr_tmp, cr_lo, cr_hi))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
						 cr, cr->blk, cr_tmp, RTS_ERROR_TEXT("cr->twin"), cr_lo, cr_hi);
				} else if (CR_NOT_ALIGNED(cr_tmp, cr_lo))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg),
						 cr, cr->blk, RTS_ERROR_TEXT("cr->twin"), cr_tmp, cr_lo, SIZEOF(cache_rec));
				} else if (cr != (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_tmp->twin))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr_tmp, cr->blk,
						 RTS_ERROR_TEXT("cr->twin->twin"), GDS_ANY_REL2ABS(csa, cr_tmp->twin), cr,
						 CALLFROM);
				}
			}
#else
			/* iosb, twin, image_count, wip_stopped are currently used in VMS only */
			if (0 != cr->twin)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->twin"), cr->twin, 0, CALLFROM);
			}
			if (0 != cr->image_count)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->image_count"), cr->image_count, 0, CALLFROM);
			}
			if ((0 != cr->epid) && caller_is_wcs_recover)
			{	/* if called from DSE CACHE -VERIFY, we do not wait for concurrent writers to finish */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
					 cr, cr->blk, RTS_ERROR_TEXT("cr->epid"), cr->epid, 0, CALLFROM);
			}
#endif
			if (FALSE != cr->wip_stopped)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("cr->wip_stopped"), cr->wip_stopped, FALSE, CALLFROM);
			}
			if (FALSE != cr->stopped)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("cr->stopped"), cr->stopped, FALSE, CALLFROM);
			}
		}
		/* loop through the cr blkques */
		for (cr0 = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array, cr_qbase = cr0; cr0 < cr_lo; cr0++)
		{
			if (cr0->blk != BT_QUEHEAD)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr0, cr0->blk,
					 RTS_ERROR_TEXT("queue head cr->blk"), cr0->blk, BT_QUEHEAD, CALLFROM);
				cr0->blk = BT_QUEHEAD;
			}
			for (cr_prev = cr0, cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr0 + cr0->blkque.fl), cnt = n_bts + 1;
			     (cr != cr0) && (cnt > 0);
			     cr_prev = cr, cnt--, cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.fl))
			{
				if (CR_NOT_IN_RANGE(cr, cr_lo, cr_hi))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg),
						 cr0, -1, cr, RTS_ERROR_TEXT("cr->blkque"), cr_lo, cr_hi);
					break;
				}
				if (CR_NOT_ALIGNED(cr, cr_lo))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg),
						 cr0, -1, RTS_ERROR_TEXT("cr->blkque"), cr, cr_lo, SIZEOF(cache_rec));
					break;
				}
				if ((cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.bl) != cr_prev)
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cr, cr->blk,
						 RTS_ERROR_TEXT("cr->blkque.bl"), (UINTPTR_T)cr->blkque.bl,
						 (sm_uc_ptr_t)cr_prev - (sm_uc_ptr_t)cr);
				}
				if (((int)(cr->blk) != CR_BLKEMPTY) && ((cr_qbase + (cr->blk % csd->bt_buckets)) != cr0))
				{
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
						 RTS_ERROR_TEXT("cr hash"), cr0 - cr_qbase, cr->blk % csd->bt_buckets, CALLFROM);
					if (caller_is_wcs_recover && !cr->stopped)
					{	/* if cr->stopped is TRUE, then the buffer was created by secshr_db_clnup(),
						 * and hence it is ok to have different hash value, but otherwise we believe
						 * the hash value and consider cr->blk to be invalid and hence make this buffer
						 * empty
						 *
						 * Possible causes of this condition are if a process gets shot (kill -9 or STOP/ID)
						 * in the midst of shuffling a cache-record from one blkque to another blkque (done
						 * through a call to shuffqth in db_csh_getn.c). Since the act of removing a
						 * cache-record from one hashqueue and adding it to another hashqueue is not
						 * atomic, we can end up with a cache-record that is not in the proper hashqueue
						 * if we get shot in the middle.
						 *
						 * Ideally we would like to dump the contents of this broken buffer to a file for
						 * later analysis. Since we hold crit now, we do not want to do that. It might be
						 * better to copy this buffer into another area in shared-memory dedicated to
						 * holding such information so a later DSE session can then dump the information.
						 *
						 * Ideally, it should be wcs_recover() that fixes the cache-record, but then the
						 * blk_que traversing logic has to be redone there in order to determine this
						 * disparity in the hash value. To avoid that we reset cr->blk here itself but
						 * do it only if called from wcs_recover().
						 */
						assert(expect_damage);
						cr->blk = CR_BLKEMPTY;
					}
				}
				(*blkque_array)[cr - cr_lo] = TRUE; /* note: this cr's blkque hash validity is already checked */
				if (0 == cr->blkque.fl)
				{	/* No point proceeding to next iteration as "cr + cr->blkque.fl" will be the same as "cr" */
					assert(expect_damage);
					ret = FALSE;
					SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cr, cr->blk,
						 RTS_ERROR_TEXT("cr->blkque.fl"), (UINTPTR_T)cr->blkque.fl, (UINTPTR_T)-1);
					break;
				}
			}
			if (cnt == 0)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
					 cr_qbase, 0, RTS_ERROR_TEXT("crque entries"), (UINTPTR_T)(n_bts + 1), (UINTPTR_T)(n_bts));
			} else if ((cr == cr0) && ((cache_rec_ptr_t)((sm_uc_ptr_t)cr + cr->blkque.bl) != cr_prev))
			{	/* at this point "cr" is cr0 and its backlink does not point to last entry in the cr0'th queue */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("crque cr_base"), (UINTPTR_T)cr->blkque.bl,
					 (sm_uc_ptr_t)cr_prev - (sm_uc_ptr_t)cr);
			}
		}
		/* scan all crs looking for non-empty cr->blks whose crs were not in any blkque */
		for (cr = cr_lo; cr < cr_hi; cr++)
		{
			if ((FALSE == (*blkque_array)[cr - cr_lo]) && ((int)(cr->blk) != CR_BLKEMPTY))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg), cr, cr->blk,
					 RTS_ERROR_TEXT("cr blkque hash"), -1, cr->blk % csd->bt_buckets, CALLFROM);
				if (caller_is_wcs_recover && !cr->stopped) /* see comment above ("cr hash") for similar handling */
				{
					assert(expect_damage);
					cr->blk = CR_BLKEMPTY;
				}
			}
		}

		que_head = &csa->acc_meth.bg.cache_state->cacheq_active;
		if ((sm_long_t)que_head % SIZEOF(que_head->fl) != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), que_head, 0, RTS_ERROR_TEXT("cacheq_active"),
				 que_head, ((sm_long_t)que_head / SIZEOF(que_head->fl)) * SIZEOF(que_head->fl));
		}
		/* loop through the active queue */
		for (cstt_prev = (cache_state_rec_ptr_t)que_head,
			cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)que_head + que_head->fl), cnt = n_bts;
			(cstt != (cache_state_rec_ptr_t)que_head) && (cnt > 0);
			cstt_prev = cstt, cnt--, cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.fl))
		{
			cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cstt - SIZEOF(cr->blkque));
			if (CR_NOT_IN_RANGE((cache_rec_ptr_t)cr, cr_lo, cr_hi))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), que_head, -1,
					 cr, RTS_ERROR_TEXT("active cstt->state_que"), cr_lo, cr_hi);
				break;
			}
			if (CR_NOT_ALIGNED(cr, cr_lo))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), que_head, -1,
					 RTS_ERROR_TEXT("active cstt->state_que"), cr, cr_lo, SIZEOF(cache_rec));
				break;
			}
			if ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.bl) != cstt_prev)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, cstt->blk,
					 RTS_ERROR_TEXT("active queue.bl"), (UINTPTR_T)cstt->state_que.bl,
					 (sm_uc_ptr_t)cstt_prev - (sm_uc_ptr_t)cstt);
			}
			if (0 == cstt->dirty)
			{
				assert(expect_damage);
				ret = FALSE;
				dummy_tn = (trans_num)TRUE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR8, 11, DB_LEN_STR(reg), cr, cstt->blk,
					 RTS_ERROR_TEXT("active cr->dirty"), &cstt->dirty, &dummy_tn, CALLFROM);
			}
			if (((0 != cstt->flushed_dirty_tn) && (cstt->dirty <= cstt->flushed_dirty_tn))
			    || (cstt->dirty > csd->trans_hist.curr_tn))
			{
				assert(expect_damage);
				ret = FALSE;
				dummy_tn = cstt->flushed_dirty_tn + 1;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE8, 9, DB_LEN_STR(reg), cstt + SIZEOF(que_head), cstt->blk,
					 &cstt->dirty, RTS_ERROR_TEXT("active dirty (tn)"), &dummy_tn, &csd->trans_hist.curr_tn);
			}
			/* if caller_is_wcs_recover, we would have waited for all writers to stop manipulating the active/wip queues
			 * and so it is ok to do the FAKE_DIRTY check. but otherwise it is not.
			 */
			if (caller_is_wcs_recover)
				cstt->dirty = FAKE_DIRTY;	/* change the flag to indicate it was found in a state queue */
			if (0 == cstt->state_que.fl)
			{	/* No point proceeding to next iteration as "cstt + cstt->state_que.fl" will be same as "cstt" */
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, cstt->blk,
					 RTS_ERROR_TEXT("active queue.fl"), (UINTPTR_T)cstt->state_que.fl, (UINTPTR_T)-1);
				break;
			}
		}
		if (cnt == 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				 que_head, 0, RTS_ERROR_TEXT("active queue entries"), (UINTPTR_T)(n_bts + 1), (UINTPTR_T)n_bts);
		} else if ((cstt == (cache_state_rec_ptr_t)que_head)
			   && ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.bl) != cstt_prev))
		{	/* at this point "cstt" is active que_head and its backlink does not point to last entry in active queue */
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, 0, RTS_ERROR_TEXT("active queue base"),
				 (UINTPTR_T)cstt->state_que.bl, (sm_uc_ptr_t)cstt_prev - (sm_uc_ptr_t)cstt);
		}

		/* loop through the wip queue */
		que_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
		if ((sm_long_t)que_head % SIZEOF(que_head->fl) != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), que_head, 0, RTS_ERROR_TEXT("cacheq_wip"),
				 que_head, ((sm_long_t)que_head / SIZEOF(que_head->fl)) * SIZEOF(que_head->fl));
		}
#ifdef VMS
		for (cstt_prev = que_head, cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)que_head + que_head->fl), cnt = n_bts;
		     (cstt != (cache_state_rec_ptr_t)que_head) && (cnt > 0);
		     cstt_prev = cstt, cnt--, cstt = (cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.fl))
		{
			cr = (cache_rec_ptr_t)((sm_uc_ptr_t)cstt - SIZEOF(cr->blkque));
			if (CR_NOT_IN_RANGE((cache_rec_ptr_t)cr, cr_lo, cr_hi))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE, 9, DB_LEN_STR(reg), que_head, -1,
					 cr, RTS_ERROR_TEXT("wip cstt->state_que"), cr_lo, cr_hi);
				break;
			}
			if (CR_NOT_ALIGNED(cr, cr_lo))
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRALIGN, 9, DB_LEN_STR(reg), que_head, -1,
					 RTS_ERROR_TEXT("wip cstt->state_que"), cr, cr_lo, SIZEOF(cache_rec));
				break;
			}
			if ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.bl) != cstt_prev)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, cstt->blk,
					 RTS_ERROR_TEXT("wip queue.bl"), (UINTPTR_T)cstt->state_que.bl,
					 (sm_uc_ptr_t)cstt_prev - (sm_uc_ptr_t)cstt);
			}
			/* Secondary failure @ ipb - not yet determined if it was a legal state or a recover problem
			 *	if (cstt->epid == 0)
			 *	{
			 *		assert(expect_damage);
			 *		ret = FALSE;
			 *		SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR, 11, DB_LEN_STR(reg),
			 *			cr, cstt->blk, RTS_ERROR_TEXT("wip cr->epid"), cstt->epid, -1, CALLFROM);
			 *	}
			 */
			if (0 == cstt->dirty)
			{
				assert(expect_damage);
				ret = FALSE;
				dummy_tn = (trans_num)TRUE;
				SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR8, 11, DB_LEN_STR(reg), cr, cstt->blk,
					 RTS_ERROR_TEXT("wip cr->dirty"), &cstt->dirty, &dummy_tn, CALLFROM);
			}
			if (((0 != cstt->flushed_dirty_tn) && (cstt->dirty <= cstt->flushed_dirty_tn))
			    || (cstt->dirty > csd->trans_hist.curr_tn))
			{
				assert(expect_damage);
				ret = FALSE;
				dummy_tn = cstt->flushed_dirty_tn + 1;
				SEND_MSG_CSA(VARLSTCNT(11) ERR_DBADDRANGE8, 9, DB_LEN_STR(reg), (int)cstt + SIZEOF(que_head),
						cstt->blk, &cstt->dirty, RTS_ERROR_TEXT("wip dirty (tn)"), &dummy_tn,
						&csd->trans_hist.curr_tn);
			}
			/* if caller_is_wcs_recover, we would have waited for all writers to stop manipulating the active/wip queues
			 * and so it is ok to do the FAKE_DIRTY check. but otherwise it is not.
			 */
			if (caller_is_wcs_recover)
				cstt->dirty = FAKE_DIRTY;	/* change the flag to indicate it was found in a state queue */
			if (0 == cstt->state_que.fl)
			{
				assert(expect_damage);
				ret = FALSE;
				SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, cstt->blk,
					 RTS_ERROR_TEXT("wip queue.fl"), (UINTPTR_T)cstt->state_que.fl, (UINTPTR_T)-1);
				break;
			}
		}
		if (cnt == 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				 que_head, 0, RTS_ERROR_TEXT("wip queue entries"), (UINTPTR_T)(n_bts + 1), (UINTPTR_T)n_bts);
		} else if ((cstt == (cache_state_rec_ptr_t)que_head)
			   && ((cache_state_rec_ptr_t)((sm_uc_ptr_t)cstt + cstt->state_que.bl) != cstt_prev))
		{	/* at this point "cstt" is wip que_head and its backlink does not point to last entry in the wip queue */
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg), cstt, 0, RTS_ERROR_TEXT("active queue base"),
				 (UINTPTR_T)cstt->state_que.bl, (sm_uc_ptr_t)cstt_prev - (sm_uc_ptr_t)cstt);
		}
#else
		if (que_head->fl != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				 que_head, 0, RTS_ERROR_TEXT("wip queue head fl"), (UINTPTR_T)que_head->fl, (UINTPTR_T)0);
			que_head->fl = 0;
		}
		if (que_head->bl != 0)
		{
			assert(expect_damage);
			ret = FALSE;
			SEND_MSG_CSA(VARLSTCNT(10) ERR_DBQUELINK, 8, DB_LEN_STR(reg),
				 que_head, 0, RTS_ERROR_TEXT("wip queue head bl"), (UINTPTR_T)que_head->bl, (UINTPTR_T)0);
			que_head->bl = 0;
		}
#endif
		/* if caller_is_wcs_recover, we would have waited for all writers to stop manipulating the active/wip queues
		 * and so it is ok to do the FAKE_DIRTY check. but otherwise it is not.
		 */
		if (caller_is_wcs_recover)
		{	/* loop through the cache_recs again to look for lost dirties */
			for (cr = cr_lo, cnt = n_bts; cnt > 0; cr++, cnt--)
			{
				if (cr->dirty == FAKE_DIRTY)
					cr->dirty = cr->flushed_dirty_tn + 1;
				else
				{
					if (0 != cr->dirty)
					{
						assert(expect_damage);
						ret = FALSE;
						dummy_tn = (trans_num)FALSE;
						SEND_MSG_CSA(VARLSTCNT(13) ERR_DBCRERR8, 11, DB_LEN_STR(reg), cr, cr->blk,
							 RTS_ERROR_TEXT("non-state cr->dirty"), &cr->dirty, &dummy_tn, CALLFROM);
					}
				}
			}
		}
	}
	SEND_MSG_CSA(VARLSTCNT(7) ERR_DBWCVERIFYEND, 5, DB_LEN_STR(reg), process_id, process_id, &csd->trans_hist.curr_tn);
	if (NULL != blkque_array)
		free(blkque_array);
	return ret;
}
