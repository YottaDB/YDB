/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_time.h"
#include "gtmimagename.h"

#ifdef UNIX

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>

#elif defined(VMS)

#include <fab.h>
#include <iodef.h>
#include <ssdef.h>

#else
#error UNSUPPORTED PLATFORM
#endif

#include "ast.h"	/* needed for DCLAST_WCS_WTSTART macro in gdsfhead.h */
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "gdsbgtr.h"
#include "gdsbml.h"
#include "filestruct.h"
#include "interlock.h"
#include "jnl.h"
#include "testpt.h"
#include "sleep_cnt.h"
#include "hashdef.h"

#ifdef UNIX
#include "eintr_wrappers.h"
GBLREF	sigset_t		blockalrm;
#endif
#include "send_msg.h"
#include "bit_set.h"
#include "bit_clear.h"
#include "relqop.h"
#include "is_proc_alive.h"
#include "mmseg.h"
#include "format_targ_key.h"
#include "gds_map_moved.h"
#include "wcs_recover.h"
#include "wcs_sleep.h"
#include "wcs_mm_recover.h"
#include "add_inter.h"


GBLREF	bool             	certify_all_blocks;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF 	sgmnt_data_ptr_t 	cs_data;
GBLREF	gd_addr			*gd_header;		/* needed in UNIX for MM file extension */
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;		/* needed in VMS for error logging in MM */
GBLREF	uint4			process_id;
GBLREF	bool			run_time;
GBLREF	testpt_struct		testpoint;
GBLREF  inctn_opcode_t          inctn_opcode;
GBLREF	boolean_t		mupip_jnl_recover;
GBLREF 	jnl_gbls_t		jgbl;
GBLREF	enum gtmImageTypes	image_type;
GBLREF	boolean_t		mu_rndwn_file_dbjnl_flush;

#ifdef DEBUG_DB64
/* if debugging large address stuff, make all memory segments allocate above 4G line */
GBLREF	sm_uc_ptr_t	next_smseg;
#else
#define next_smseg	NULL
#endif

void		wcs_recover(gd_region *reg)
{
	bt_rec_ptr_t		bt;
	cache_rec_ptr_t		cr, cr_alt, cr_top, hash_hdr;
	cache_que_head_ptr_t	active_head, hq, wip_head, wq;
	gd_region		*save_reg;
	que_ent_ptr_t		back_link; /* should be crit & not need interlocked ops. */
	sgmnt_data_ptr_t	csd;
	sgmnt_addrs		*csa;
	bool			blk_used, change_bmm;
	int4			bml_full, dummy_errno;
	uint4			jnl_status, epid;
	int			bt_buckets;
	inctn_opcode_t          save_inctn_opcode;
	unsigned int		bplmap, lcnt, total_blks;

	error_def(ERR_BUFRDTIMEOUT);
	error_def(ERR_DBCCERR);
	error_def(ERR_DBCNTRLERR);
	error_def(ERR_DBDANGER);
	error_def(ERR_ERRCALL);
	error_def(ERR_INVALIDRIP);
	error_def(ERR_STOPTIMEOUT);
	error_def(ERR_TEXT);

	save_reg = gv_cur_region;	/* protect against [at least] M LOCK code which doesn't maintain cs_addrs and cs_data */
	TP_CHANGE_REG(reg);	/* which are needed by called routines such as wcs_wtstart and wcs_mm_recover */
	if (dba_mm == reg->dyn.addr->acc_meth)	 /* MM uses wcs_recover to remap the database in case of a file extension */
	{
		wcs_mm_recover(reg);
		TP_CHANGE_REG(save_reg);
		return;
	}
	csa = &FILE_INFO(reg)->s_addrs;
	csd = csa->hdr;
	assert(csa->now_crit || csd->clustered);
	/* ??? this should probably issue an error and
	 * grab crit on the assumption that it is properly called and something will presently release it */
	csd->wc_blocked = TRUE;
	for (lcnt=1;  (0 < csa->nl->in_wtstart) && (lcnt <= MAXWTSTARTWAIT);  lcnt++)
	{	/* wait for any in wcs_wtstart to finish */
		/* if this loop hits the limit, or in_wtstart goes negative wcs_verify reports and clears in_wtstart */
		wcs_sleep(lcnt);
	}
	if (wcs_verify(reg, TRUE))
	{	/* if it passes verify, then recover can't help ??? what to do */
		BG_TRACE_PRO_ANY(csa, wc_blocked_wcs_verify_passed);
		send_msg(VARLSTCNT(4) ERR_DBCNTRLERR, 2, DB_LEN_STR(reg));
	}
	change_bmm = FALSE;
	bt_refresh(csa);
	/* the following queue head initializations depend on the wc_blocked mechanism for protection from wcs_wtstart */
	wip_head = &csa->acc_meth.bg.cache_state->cacheq_wip;
	memset(wip_head, 0, sizeof(cache_que_head));
	active_head = &csa->acc_meth.bg.cache_state->cacheq_active;
	memset(active_head, 0, sizeof(cache_que_head));
	UNIX_ONLY(wip_head = active_head);	/* all inserts into wip_que in VMS should be done in active_que in UNIX */
	UNIX_ONLY(SET_LATCH_GLOBAL(&active_head->latch, LOCK_AVAILABLE));
	csa->nl->wcs_active_lvl = 0;
	csa->nl->wc_in_free = 0;
	bplmap = csd->bplmap;
	hash_hdr = (cache_rec_ptr_t)csa->acc_meth.bg.cache_state->cache_array;
	bt_buckets = csd->bt_buckets;
	for (cr = hash_hdr, cr_top = cr + bt_buckets;  cr < cr_top;  cr++)
		cr->blkque.fl = cr->blkque.bl = 0;	/* take no chances that the blkques are messed up */
	for (cr = cr_top, cr_top = cr + csd->n_bts;  cr < cr_top;  cr++)
	{
		for (lcnt = 1;  (-1 != cr->read_in_progress);  lcnt++)
		{	/* very similar code appears elsewhere and perhaps should be common */
			if (cr->read_in_progress < -1)
			{
				send_msg(VARLSTCNT(4) ERR_INVALIDRIP, 2, DB_LEN_STR(reg));
				INTERLOCK_INIT(cr);
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				assert(cr->r_epid == 0);
				assert(!cr->dirty);
			} else  if ((0 != cr->r_epid)
					&& ((cr->r_epid == process_id) || (FALSE == is_proc_alive(cr->r_epid, cr->image_count))))
			{
				INTERLOCK_INIT(cr);			/* Process gone, release that process's lock */
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
	   		} else
			{
				if (1 == lcnt)
					epid = cr->r_epid;
				else  if (BUF_OWNER_STUCK < lcnt)
				{
					if ((0 != cr->r_epid) && (epid != cr->r_epid))
						GTMASSERT;
					if (0 != epid)
					{	/* process still active, but not playing fair */
						send_msg(VARLSTCNT(8) ERR_BUFRDTIMEOUT, 6, process_id,
									cr->blk, cr, epid, DB_LEN_STR(reg));
						send_msg(VARLSTCNT(4) ERR_TEXT, 2, LEN_AND_LIT("Buffer forcibly seized"));
					}
					INTERLOCK_INIT(cr);
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					continue;
				}
    				wcs_sleep(lcnt);
			}
		}
		cr->blkque.fl = cr->blkque.bl = 0;		/* take no chances that the blkques are messed up */
		cr->state_que.fl = cr->state_que.bl = 0;	/* take no chances that the state_ques are messed up */
		cr->r_epid = 0;		/* the processing above should make this appropriate */
		cr->in_cw_set = FALSE;	/* this has crit and is here, so in_cw_set must no longer be true */
		if (cr->wip_stopped)
		{
			UNIX_ONLY(assert(FALSE));
			for (lcnt = 1; (0 == cr->iosb[0]) && is_proc_alive(cr->epid, cr->image_count); lcnt++)
			{
				if (1 == lcnt)
					epid = cr->epid;
				else  if (BUF_OWNER_STUCK < lcnt)
				{
					if ((0 != cr->epid) && (epid != cr->epid))
						GTMASSERT;
					if (0 != epid)
					{	/* process still active, but not playing fair */
						send_msg(VARLSTCNT(5)
							ERR_STOPTIMEOUT, 3, epid, DB_LEN_STR(reg));
						send_msg(VARLSTCNT(4)
							ERR_TEXT, 2, sizeof("Buffer forcibly seized"), "Buffer forcibly seized");
						cr->epid = 0;
					}
					continue;
				}
    				wcs_sleep(lcnt);
			}
			if (0 == cr->iosb[0])
			{	/* if it's abandonned wip_stopped, treat it as a WRT_STRT_PNDNG */
				cr->iosb[0] = WRT_STRT_PNDNG;
				cr->epid = 0;
				cr->image_count = 0;
			}	/* otherwise the iosb[0] should suffice */
			cr->wip_stopped = FALSE;
		}
		if (0 != cr->twin)
		{	/* clean up any old twins */
			UNIX_ONLY(assert(FALSE));		/* Unix doesn't have twinning currently. */
			cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
			assert(((cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->twin)) == cr);
			assert((0 == cr->bt_index) || (0 == cr_alt->bt_index));		/* at least one zero */
			assert((0 != cr->bt_index) || (0 != cr_alt->bt_index));		/* at least one non-zero */
			cr_alt->twin = cr->twin = 0;
		}
		if (JNL_ENABLED(csd))
		{
			if (cr->jnl_addr > csa->jnl->jnl_buff->freeaddr)
			{
				assert(0 == cr->dirty);
				cr->jnl_addr = csa->jnl->jnl_buff->freeaddr;
			}
		} else
		{	/* cr->jnl_addr can be non-zero at this point in time because of a JNL_ENABLED to JNL_ALLOWED online
			 * state transition. in this case, reset cr->jnl_addr to 0. even otherwise, just be safe.
			 */
			cr->jnl_addr = 0;
		}
		if (cr->stopped)
		{	/* cache record attached to a buffer built by secshr_db_clnup: finish work; clearest case: do it 1st */
			assert(CR_BLKEMPTY != cr->blk);
			if (certify_all_blocks && !cert_blk(cr->blk, (blk_hdr_ptr_t)GDS_REL2ABS(cr->buffaddr), 0))
				GTMASSERT;
			assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr));
			if ((cr->blk / bplmap) * bplmap == cr->blk)
			{	/* it's a bitmap */
				if ((csa->ti->total_blks / bplmap) * bplmap == cr->blk)
					total_blks = csa->ti->total_blks - cr->blk;
				else
					total_blks = bplmap;
				bml_full = bml_find_free(0, (sm_uc_ptr_t)(GDS_ANY_REL2ABS(csa, cr->buffaddr)) + sizeof(blk_hdr),
						total_blks, &blk_used);
				if (NO_FREE_SPACE == bml_full)
				{
					bit_clear(cr->blk / bplmap, csd->master_map);
					if (cr->blk > csa->nl->highest_lbm_blk_changed)
						csa->nl->highest_lbm_blk_changed = cr->blk;
					change_bmm = TRUE;
				} else if (!(bit_set(cr->blk / bplmap, csd->master_map)))
				{
					if (cr->blk > csa->nl->highest_lbm_blk_changed)
						csa->nl->highest_lbm_blk_changed = cr->blk;
					change_bmm = TRUE;
				}
			}	/* end of bitmap processing */
			bt = bt_put(reg, cr->blk);
			if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
				GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
			bt->killtn = csa->ti->curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID != bt->cache_index)
			{	/* the bt already identifies another cache entry with this block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
					> ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				cr_alt->bt_index = 0;				/* cr is more recent */
				assert(LATCH_CLEAR <= WRITE_LATCH_VAL(cr_alt) && LATCH_CONFLICT >= WRITE_LATCH_VAL(cr_alt));
				if (UNIX_ONLY(FALSE &&) LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
				{	/* the previous entry is of interest to some process and therefore must be WIP:
					 * twin and make this (cr->stopped) cache record the active one */
					assert(0 == cr_alt->twin);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					WRITE_LATCH_VAL(cr_alt) = LATCH_CONFLICT;	/* semaphore state of a wip twin */
				} else
				{	/* the other copy is less recent and not WIP, so discard it */
					if ((cr_alt < cr) && cr_alt->state_que.fl)
					{	/* cr_alt has already been processed and is in the state_que. hence remove it */
						wq = (cache_que_head_ptr_t)((sm_uc_ptr_t)&cr_alt->state_que + cr_alt->state_que.fl);
						assert(0 == (((unsigned int)wq) % sizeof(que_ent)));
						assert((unsigned int)wq + wq->bl == (unsigned int)&cr_alt->state_que);
						back_link = (que_ent_ptr_t)remqt((que_ent_ptr_t)wq);
						assert(EMPTY_QUEUE != back_link);
						SUB_ENT_FROM_ACTIVE_QUE_CNT(&csa->nl->wcs_active_lvl, &csa->nl->wc_var_lock);
						assert(0 <= csa->nl->wcs_active_lvl);
						assert(back_link == (que_ent *)&cr_alt->state_que);
					}
					UNIX_ONLY(assert(!cr_alt->twin));
					cr->twin = cr_alt->twin;		/* existing cache record may have a twin */
					cr_alt->cycle++; /* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr_alt->blk = CR_BLKEMPTY;
					cr_alt->dirty = 0;
					cr_alt->in_tend = FALSE;
					WRITE_LATCH_VAL(cr_alt) = LATCH_CLEAR;
					cr_alt->iosb[0] = 0;
					cr_alt->jnl_addr = 0;
					cr_alt->refer = FALSE;
					cr_alt->twin = 0;
					csa->nl->wc_in_free++;
					if (0 != cr->twin)
					{	/* inherited a WIP twin from cr_alt, transfer the twin's affections */
						cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr->twin);
						assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
							> ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
						assert(LATCH_CONFLICT == WRITE_LATCH_VAL(cr_alt)); /* semaphore for wip twin */
						assert(0 == cr_alt->bt_index);
						cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					}
				}	/* if (LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt)) */
			}	/* if (CR_NOTVALID == cr_alt) */
			bt->cache_index = GDS_ANY_ABS2REL(csa, cr);
			cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
			cr->dirty = csa->ti->curr_tn;
			cr->epid = 0;
			cr->image_count = 0;
			cr->in_tend = FALSE;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			assert(0 == cr->iosb[0]);
			cr->iosb[0] = 0;
			cr->refer = TRUE;
			cr->stopped = FALSE;
			cr->wip_stopped = FALSE;
			hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
			insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
			ADD_ENT_TO_ACTIVE_QUE_CNT(&csa->nl->wcs_active_lvl, &csa->nl->wc_var_lock);
		} else  if ((CR_BLKEMPTY == cr->blk) || cr->data_invalid || (0 == cr->dirty)
				|| ((0 != cr->iosb[0]) && (0 == cr->bt_index)))
		{	/* cache record has no valid buffer attached, or its contents are in the database,
			 * or it has a more recent twin so we don't even have to care how its write terminated */
			cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
			cr->blk = CR_BLKEMPTY;
			cr->bt_index = 0;
			cr->data_invalid = FALSE;
			cr->dirty = 0;
			cr->epid = 0;
			cr->image_count = 0;
			cr->in_tend = FALSE;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			cr->iosb[0] = 0;
			cr->jnl_addr = 0;
			cr->refer = FALSE;
			cr->wip_stopped = FALSE;
			csa->nl->wc_in_free++;
		} else if (cr->in_tend)
		{	/* caught by a failure while in bg_update, and less recent than a cache record created by secshr_db_clnup */
			if (UNIX_ONLY(FALSE &&) (LATCH_CONFLICT == WRITE_LATCH_VAL(cr)) && (0 == cr->iosb[0])
						&& ((FALSE == cr->wip_stopped) || is_proc_alive(cr->epid, cr->image_count)))
			{	/* must be WIP, with a currently active write */
				assert(LATCH_CONFLICT >= WRITE_LATCH_VAL(cr));
				hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
				WRITE_LATCH_VAL(cr) = LATCH_SET;
				bt = bt_put(reg, cr->blk);
				if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
					GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
				bt->killtn = csa->ti->curr_tn;	/* be safe; don't know when was last kill after recover */
				if (CR_NOTVALID == bt->cache_index)
				{	/* no previous entry for this block; more recent cache record will twin when processed */
					cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
					bt->cache_index = GDS_ANY_ABS2REL(csa, cr);
					insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				} else
				{	/* form the twin with the previous (and more recent) cache record */
					cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						< ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
					assert(0 == cr_alt->twin);
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					cr->bt_index = 0;
					insqt((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				}
				insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)wip_head);
				UNIX_ONLY(cr->epid = 0);
			} else
			{	/* the [current] in_tend cache record is no longer of value and can be discarded */
				cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
				cr->blk = CR_BLKEMPTY;
				cr->bt_index = 0;
				cr->dirty = 0;
				cr->epid = 0;
				cr->image_count = 0;
				WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
				cr->iosb[0] = 0;
				cr->jnl_addr = 0;
				cr->wip_stopped = FALSE;
				csa->nl->wc_in_free++;
			}
			UNIX_ONLY(send_msg(VARLSTCNT(4) ERR_DBDANGER, 2, DB_LEN_STR(reg)));
			cr->in_tend = FALSE;
			cr->refer = FALSE;
		} else if ((LATCH_SET > WRITE_LATCH_VAL(cr)) || (WRT_STRT_PNDNG == cr->iosb[0]))
		{	/* no process has an interest */
			bt = bt_put(reg, cr->blk);
			if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
				GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
			bt->killtn = csa->ti->curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID == bt->cache_index)
			{	/* no previous entry for this block */
				bt->cache_index = GDS_ANY_ABS2REL(csa, cr);
				cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
				cr->refer = TRUE;
				hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
				insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
				insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
				ADD_ENT_TO_ACTIVE_QUE_CNT(&csa->nl->wcs_active_lvl, &csa->nl->wc_var_lock);
			} else
			{	/* the bt already has an entry for the block */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				if (UNIX_ONLY(FALSE &&) LATCH_CLEAR < WRITE_LATCH_VAL(cr_alt))
				{	/* the previous cache record is WIP, and the current cache record is the more recent twin */
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						> ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					assert(WRT_STRT_PNDNG != cr->iosb[0]);
					cr_alt->bt_index = 0;
					WRITE_LATCH_VAL(cr_alt) = LATCH_CONFLICT;
					cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
					cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
					bt->cache_index = GDS_ANY_ABS2REL(csa, cr);
					cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
					cr->refer = TRUE;
					hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
					insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
					insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)active_head);
					ADD_ENT_TO_ACTIVE_QUE_CNT(&csa->nl->wcs_active_lvl, &csa->nl->wc_var_lock);
				} else
				{	/* previous cache record is more recent from a cr->stopped record made by sechsr_db_clnup:
					 * discard this copy as it is old */
					assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
						< ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
					assert(LATCH_CLEAR == WRITE_LATCH_VAL(cr_alt));
					cr->cycle++;	/* increment cycle whenever blk number changes (tp_hist depends on this) */
					cr->blk = CR_BLKEMPTY;
					cr->bt_index = 0;
					cr->dirty = 0;
					cr->jnl_addr = 0;
					cr->refer = FALSE;
					csa->nl->wc_in_free++;
				}
			}
			cr->epid = 0;
			cr->image_count = 0;
			WRITE_LATCH_VAL(cr) = LATCH_CLEAR;
			assert((0 == cr->iosb[0]) || (WRT_STRT_PNDNG == cr->iosb[0]));
			cr->iosb[0] = 0;
		} else
		{	/* not in_tend and interlock.semaphore is not LATCH_CLEAR so cache record must be WIP */
			assert(LATCH_CONFLICT >= WRITE_LATCH_VAL(cr));
			VMS_ONLY(WRITE_LATCH_VAL(cr) = LATCH_SET;)
			UNIX_ONLY(WRITE_LATCH_VAL(cr) = LATCH_CLEAR;)
			hq = (cache_que_head_ptr_t)(hash_hdr + (cr->blk % bt_buckets));
			bt = bt_put(reg, cr->blk);
			if (NULL == bt)		/* NULL value is only possible if wcs_get_space in bt_put fails */
				GTMASSERT;	/* That is impossible here since we have called bt_refresh above */
			bt->killtn = csa->ti->curr_tn;	/* be safe; don't know when was last kill after recover */
			if (CR_NOTVALID == bt->cache_index)
			{	/* no previous entry for this block */
				bt->cache_index = GDS_ANY_ABS2REL(csa, cr);
				cr->bt_index = GDS_ANY_ABS2REL(csa, bt);
				cr->refer = TRUE;
				insqh((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
			} else
			{	/* previous cache record must be more recent as this one is WIP */
				cr_alt = (cache_rec_ptr_t)GDS_ANY_REL2ABS(csa, bt->cache_index);
				assert(((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr->buffaddr))->tn
					< ((blk_hdr_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->buffaddr))->tn);
				assert((bt_rec_ptr_t)GDS_ANY_REL2ABS(csa, cr_alt->bt_index) == bt);
				assert(WRT_STRT_PNDNG != cr->iosb[0]);
				assert(FALSE == cr_alt->wip_stopped);
				cr_alt->twin = GDS_ANY_ABS2REL(csa, cr);
				cr->twin = GDS_ANY_ABS2REL(csa, cr_alt);
				cr->bt_index = 0;
				cr->refer = FALSE;
				insqt((que_ent_ptr_t)&cr->blkque, (que_ent_ptr_t)hq);
			}
			insqt((que_ent_ptr_t)&cr->state_que, (que_ent_ptr_t)wip_head);
			UNIX_ONLY(cr->epid = 0);
			UNIX_ONLY(ADD_ENT_TO_ACTIVE_QUE_CNT(&csa->nl->wcs_active_lvl, &csa->nl->wc_var_lock);)
		}	/* end of processing for a single cache record */
	}	/* end of processing all cache records */
	if (change_bmm)
	{
		csa->ti->mm_tn++;
		if (!reg->read_only)
			fileheader_sync(reg);
	}
	if (FALSE == wcs_verify(reg, FALSE))
		GTMASSERT;
	assert(csa->ti->curr_tn == csa->ti->early_tn || MUPIP_IMAGE == image_type);
	/* skip INCTN processing in case called from mu_rndwn_file().
	 * if called from mu_rndwn_file(), we have standalone access to shared memory so no need to increment db curr_tn
	 * or write inctn (since no concurrent GT.M process is present in order to restart because of this curr_tn change)
	 */
	if (!mu_rndwn_file_dbjnl_flush)
	{
		if (JNL_ENABLED(csd))
		{
			assert(csa->jnl->region == reg);
			csa->jnl->region = reg;	/* Make sure that in pro we make jnl_ensure happy if it's not initialized */
			if (!jgbl.forw_phase_recovery && (csa->ti->curr_tn == csa->ti->early_tn))
				JNL_SHORT_TIME(jgbl.gbl_jrec_time); /* needed for jnl_put_jrt_pini() and jnl_write_inctn_rec() */
			assert(jgbl.gbl_jrec_time);
			jnl_status = jnl_ensure_open();
			if (0 == jnl_status)
			{
				if (0 == csa->jnl->pini_addr)
					jnl_put_jrt_pini(csa);
				save_inctn_opcode = inctn_opcode; /* in case caller does not expect inctn_opcode
												to be changed here */
				inctn_opcode = inctn_wcs_recover;
				jnl_write_inctn_rec(csa);
				inctn_opcode = save_inctn_opcode;
			} else
				jnl_file_lost(csa->jnl, jnl_status);
		}
		if (!mupip_jnl_recover || JNL_ENABLED(csd))
			csa->ti->early_tn = ++csa->ti->curr_tn;	/* do not increment transaction number for forward recovery */
	}
	csa->wbuf_dqd = FALSE;	/* reset this so the wcs_wtstart below will work */
	csd->wc_blocked = FALSE;
	if (!reg->read_only)
		DCLAST_WCS_WTSTART(reg, 0, dummy_errno);
	TP_CHANGE_REG(save_reg);
	return;
}

#ifdef UNIX

void	wcs_mm_recover(gd_region *reg)
{
	int			status, mm_prot;
        struct stat     	stat_buf;
	sm_uc_ptr_t		old_base[2];
	sigset_t        	savemask;
	boolean_t       	need_to_restore_mask = FALSE, was_crit;
	unix_db_info		*udi;

	error_def(ERR_DBFILERR);

	assert(&FILE_INFO(reg)->s_addrs == cs_addrs);
	assert(cs_addrs->hdr == cs_data);
	if (!(was_crit = cs_addrs->now_crit) && !(cs_addrs->hdr->clustered))
		grab_crit(gv_cur_region);
	if (cs_addrs->total_blks == cs_addrs->ti->total_blks)
	{
		/* I am the one who actually did the extension, don't need to remap again */
		if (!was_crit)
			rel_crit(gv_cur_region);
		return;
	}

	mm_prot = cs_addrs->read_write ? (PROT_READ | PROT_WRITE) : PROT_READ;

	/* Block SIGALRM to ensure cs_data and cs_addrs are always in-sync / No IO in this period */
	sigprocmask(SIG_BLOCK, &blockalrm, &savemask);
	old_base[0] = cs_addrs->db_addrs[0];
	old_base[1] = cs_addrs->db_addrs[1];
	status = munmap((caddr_t)old_base[0], (size_t)(old_base[1] - old_base[0]));
	if (-1 != status)
	{
		udi = FILE_INFO(gv_cur_region);
		FSTAT_FILE(udi->fd, &stat_buf, status);
#ifdef DEBUG_DB64
		rel_mmseg((caddr_t)old_base[0]);
		status = ((sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)get_mmseg((size_t)stat_buf.st_size),
										(size_t)stat_buf.st_size,
										mm_prot,
										GTM_MM_FLAGS, udi->fd, (off_t)0)));
#else
		status = ((sm_long_t)(cs_addrs->db_addrs[0] = (sm_uc_ptr_t)mmap((caddr_t)NULL,
										(size_t)stat_buf.st_size,
										mm_prot,
										GTM_MM_FLAGS, udi->fd, (off_t)0)));
#endif
	}
	if (-1 == status)
	{
		sigprocmask(SIG_SETMASK, &savemask, NULL);
		rel_crit(gv_cur_region);
		rts_error(VARLSTCNT(5) ERR_DBFILERR, 2, DB_LEN_STR(reg), errno);
	}
#ifdef DEBUG_DB64
	put_mmseg((caddr_t)(cs_addrs->db_addrs[0]), (size_t)stat_buf.st_size);
#endif
	cs_data = cs_addrs->hdr = (sgmnt_data_ptr_t)cs_addrs->db_addrs[0];
	cs_addrs->db_addrs[1] = cs_addrs->db_addrs[0] + stat_buf.st_size - 1;
	cs_addrs->bmm = cs_data->master_map;
	cs_addrs->acc_meth.mm.base_addr = (sm_uc_ptr_t)((sm_uc_ptr_t)cs_data
							+ (cs_data->start_vbn - 1) * DISK_BLOCK_SIZE);
	bt_init(cs_addrs);
	if (cs_addrs->db_addrs[0] != old_base[0])
		gds_map_moved(gd_header->tab_ptr, cs_addrs->db_addrs[0], old_base[0], old_base[1]);
	cs_addrs->total_blks = cs_addrs->ti->total_blks;
	if (!was_crit)
		rel_crit(gv_cur_region);
	sigprocmask(SIG_SETMASK, &savemask, NULL);
	return;
}

#elif defined(VMS)

void	wcs_mm_recover(gd_region *reg)
{
	unsigned char		*end, buff[MAX_STRLEN];

	error_def(ERR_GBLOFLOW);
	error_def(ERR_GVIS);

	assert(&FILE_INFO(reg)->s_addrs == cs_addrs);
	assert(cs_addrs->now_crit);
	assert(cs_addrs->hdr == cs_data);
	/* but it isn't yet implemented on VMS */
	rel_crit(gv_cur_region);
	if (NULL == (end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE)))
		end = &buff[MAX_ZWR_KEY_SZ - 1];
	rts_error(VARLSTCNT(6) ERR_GBLOFLOW, 0, ERR_GVIS, 2, end - buff, buff);
	return;
}

#else
#error UNSUPPORTED PLATFORM
#endif
