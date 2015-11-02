/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_inet.h"	/* Required for gtmsource.h */

#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "jnl.h"
#include "copy.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "interlock.h"

/* Include prototypes */
#include "gvcst_kill_blk.h"
#include "t_qread.h"
#include "t_end.h"
#include "t_retry.h"
#include "t_begin.h"
#include "gvcst_expand_free_subtree.h"
#include "gvcst_protos.h"	/* for gvcst_kill,gvcst_search prototype */
#include "rc_cpt_ops.h"
#include "add_inter.h"
#include "sleep_cnt.h"
#include "wcs_sleep.h"
#include "wbox_test_init.h"
#include "memcoherency.h"

GBLREF	boolean_t		is_updproc;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	boolean_t		pool_init;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	uint4			update_array_size, cumul_update_array_size; /* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF	kill_set		*kill_set_tail;
GBLREF	short			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t 		kip_incremented;
GBLREF	boolean_t		need_kip_incr;
GBLREF	boolean_t		is_replicator;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	int4			update_trans;
GBLREF	jnlpool_addrs		jnlpool;

void	gvcst_kill(bool do_subtree)
{
	bool			clue, flush_cache;
	boolean_t		left_extra, right_extra;
	boolean_t		actual_update, next_fenced_was_null, write_logical_recs;
	int4			prev_update_trans, sleep_counter;
	cw_set_element		*tp_cse;
	enum cdb_sc		cdb_status;
	int			lev;
	uint4			segment_update_array_size;
	jnl_action		*ja;
	jnl_format_buffer	*jfb;
	kill_set		kill_set_head, *ks, *temp_ks;
	srch_hist		*alt_hist;
	srch_blk_status		*left,*right;
	srch_rec_status		*left_rec_stat, local_srch_rec;
	sm_uc_ptr_t		jnlpool_instfilename;
	unsigned char		instfilename_copy[MAX_FN_LEN + 1];
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;

	error_def(ERR_SCNDDBNOUPD);
	error_def(ERR_REPLINSTMISMTCH);

	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
	if (REPL_ALLOWED(csd) && is_replicator)
	{
		if (FALSE == pool_init)
			jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);
		assert(pool_init);
		if (!csa->replinst_matches_db)
		{
			if (jnlpool_ctl->upd_disabled && !is_updproc)
			{	/* Updates are disabled in this journal pool. Detach from journal pool and issue error. */
				assert(NULL != jnlpool.jnlpool_ctl);
				jnlpool_detach();
				assert(NULL == jnlpool.jnlpool_ctl);
				assert(FALSE == pool_init);
				rts_error(VARLSTCNT(1) ERR_SCNDDBNOUPD);
			}
			UNIX_ONLY(jnlpool_instfilename = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.instfilename;)
			VMS_ONLY(jnlpool_instfilename = (sm_uc_ptr_t)jnlpool_ctl->jnlpool_id.gtmgbldir;)
			if (STRCMP(cnl->replinstfilename, jnlpool_instfilename))
			{	/* Replication instance file mismatch. Issue error. But before that detach from journal pool.
				 * Copy replication instance file name in journal pool to temporary memory before detaching.
				 */
				UNIX_ONLY(assert(sizeof(instfilename_copy) == sizeof(jnlpool_ctl->jnlpool_id.instfilename));)
				VMS_ONLY(assert(sizeof(instfilename_copy) == sizeof(jnlpool_ctl->jnlpool_id.gtmgbldir));)
				memcpy(&instfilename_copy[0], jnlpool_instfilename, sizeof(instfilename_copy));
				assert(NULL != jnlpool.jnlpool_ctl);
				jnlpool_detach();
				assert(NULL == jnlpool.jnlpool_ctl);
				assert(FALSE == pool_init);
				rts_error(VARLSTCNT(8) ERR_REPLINSTMISMTCH, 6, LEN_AND_STR(instfilename_copy),
					DB_LEN_STR(gv_cur_region), LEN_AND_STR(cnl->replinstfilename));
			}
			csa->replinst_matches_db = TRUE;
		}
	}
	clue = (0 != gv_target->clue.end);
	if (0 == dollar_tlevel)
	{
		kill_set_head.next_kill_set = NULL;
		if (jnl_fence_ctl.level)	/* next_fenced_was_null is reliable only if we are in ZTransaction */
			next_fenced_was_null = (NULL == csa->next_fenced) ? TRUE : FALSE;
	} else
		prev_update_trans = sgm_info_ptr->update_trans;
	T_BEGIN_SETORKILL_NONTP_OR_TP(ERR_GVKILLFAIL);
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	for (;;)
	{
		assert(t_tries < CDB_STAGNATE || csa->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		if (0 == dollar_tlevel)
		{
			CHECK_AND_RESET_UPDATE_ARRAY;	/* reset update_array_ptr to update_array */
			kill_set_tail = &kill_set_head;
			for (ks = &kill_set_head;  NULL != ks;  ks = ks->next_kill_set)
				ks->used = 0;
		} else
		{
			segment_update_array_size = UA_NON_BM_SIZE(csd);
			ENSURE_UPDATE_ARRAY_SPACE(segment_update_array_size);
		}
		if (cdb_sc_normal != (cdb_status = gvcst_search(gv_currkey, NULL)))
			goto retry;
		assert(gv_altkey->top == gv_currkey->top);
		assert(gv_altkey->top == gv_keysize);
		assert(gv_currkey->end < gv_currkey->top);
		memcpy(gv_altkey, gv_currkey, sizeof(gv_key) + gv_currkey->end);
		if (do_subtree)
		{
			gv_altkey->base[gv_altkey->end - 1] = 1;
			assert(0 == gv_altkey->base[gv_altkey->end]);
			gv_altkey->base[++gv_altkey->end] = 0;
		} else
		{
			gv_altkey->base[gv_altkey->end] = 1;
			gv_altkey->base[++gv_altkey->end] = 0;
			gv_altkey->base[++gv_altkey->end] = 0;
		}
		alt_hist = gv_target->alt_hist;
		if (cdb_sc_normal != (cdb_status = gvcst_search(gv_altkey, alt_hist)))
			goto retry;
		if (alt_hist->depth != gv_target->hist.depth)
		{
			cdb_status = cdb_sc_badlvl;
			goto retry;
		}
		right_extra = FALSE;
		left_extra = TRUE;
		actual_update = FALSE;
		for (lev = 0; 0 != gv_target->hist.h[lev].blk_num; ++lev)
		{
			left = &gv_target->hist.h[lev];
			right = &alt_hist->h[lev];
			assert(0 != right->blk_num);
			left_rec_stat = left_extra ? &left->prev_rec : &left->curr_rec;
			if (left->blk_num == right->blk_num)
			{
				cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat, right->curr_rec,
								right_extra, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = TRUE;
				if (cdb_sc_normal == cdb_status)
					break;
				gv_target->clue.end = 0;	/* If need to go up from leaf (or higher),
								   history will cease to be valid */
				if (clue)
				{	/* Clue history valid only for data block, need to re-search */
					clue = FALSE;
					goto research;
				}
				if (cdb_sc_delete_parent != cdb_status)
					goto retry;
				left_extra = right_extra
					   = TRUE;
			} else
			{
				gv_target->clue.end = 0;	/* If more than one block involved,
								   history will cease to be valid */
				if (clue)
				{	/* Clue history valid only for data block, need to re-search */
					clue = FALSE;
					goto research;
				}
				local_srch_rec.offset = ((blk_hdr_ptr_t)left->buffaddr)->bsiz;
				local_srch_rec.match = 0;
				cdb_status = gvcst_kill_blk(left, lev, gv_currkey, *left_rec_stat, local_srch_rec, FALSE, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (left->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = TRUE;
				if (cdb_sc_normal == cdb_status)
					left_extra = FALSE;
				else if (cdb_sc_delete_parent == cdb_status)
				{
					left_extra = TRUE;
					cdb_status = cdb_sc_normal;
				} else
					goto retry;
				local_srch_rec.offset = local_srch_rec.match
						      = 0;
				cdb_status = gvcst_kill_blk(right, lev, gv_altkey, local_srch_rec, right->curr_rec,
								right_extra, &tp_cse);
				assert(!dollar_tlevel || (NULL == tp_cse) || (right->cse == tp_cse));
				assert( dollar_tlevel || (NULL == tp_cse));
				if (tp_cse)
					actual_update = TRUE;
				if (cdb_sc_normal == cdb_status)
					right_extra = FALSE;
				else if (cdb_sc_delete_parent == cdb_status)
				{
					right_extra = TRUE;
					cdb_status = cdb_sc_normal;
				} else
					goto retry;
			}
		}
		if (!dollar_tlevel)
		{
			assert(FALSE == actual_update);
			actual_update = (0 != cw_set_depth); /* for non-TP, tp_cse is NULL even if cw_set_depth is non-zero */
			/* reset update_trans to TRUE in case it got set to FALSE in previous retry */
			/* do not treat redundant KILL as an update-transaction */
			update_trans = actual_update;
		} else
		{
			assert(!actual_update || sgm_info_ptr->cw_set_depth);
			/* no need to reset sgm_info_ptr->update_trans in case actual_update is TRUE (like is done above for
			 * non-TP) as retry in TP will have caused flow of control to restart from the beginning of transaction
			 * and tp_clean_up would have reset sgm_info_ptr->update_trans anyways.
			 */
			if (!actual_update)
				sgm_info_ptr->update_trans = prev_update_trans;	/* restore status prior to redundant KILL */
		}
		if ((write_logical_recs = JNL_WRITE_LOGICAL_RECS(csa)) && actual_update)
		{	/* Maintain journal records only if the kill actually resulted in an update. */
			if (0 == dollar_tlevel)
			{
				jfb = non_tp_jfb_ptr; /* Already malloced in gvcst_init() */
				jgbl.cumul_jnl_rec_len = 0;
				DEBUG_ONLY(jgbl.cumul_index = jgbl.cu_jnl_index = 0;)
			} else
			{
				jfb = (jnl_format_buffer *)get_new_element(sgm_info_ptr->jnl_list, 1);
				jfb->next = NULL;
				assert(NULL == *sgm_info_ptr->jnl_tail);
				*sgm_info_ptr->jnl_tail = jfb;
				sgm_info_ptr->jnl_tail = &jfb->next;
			}
			ja = &(jfb->ja);
			ja->key = gv_currkey;
			ja->val = NULL;
			if (do_subtree)
				ja->operation = JNL_KILL;
			else
				ja->operation = JNL_ZKILL;
			jnl_format(jfb);
			jgbl.cumul_jnl_rec_len += jfb->record_size;
			assert(0 == jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY);
			DEBUG_ONLY(jgbl.cumul_index++;)
		}
		flush_cache = FALSE;
		if (0 == dollar_tlevel)
		{
			if ((0 != csd->dsid) && (0 < kill_set_head.used)
				&& gv_target->hist.h[1].blk_num != alt_hist->h[1].blk_num)
			{	/* multi-level delete */
				rc_cpt_inval();
				flush_cache = TRUE;
			}
			if (0 < kill_set_head.used)		/* increase kill_in_prog */
			{
				need_kip_incr = TRUE;
				if (!csa->now_crit)	/* Do not sleep while holding crit */
					WAIT_ON_INHIBIT_KILLS(cnl, MAXWAIT2KILL);
			}
			if ((trans_num)0 == t_end(&gv_target->hist, alt_hist))
			{	/* In case this is MM and t_end caused a database extension, reset csd */
				assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
				csd = cs_data;
				if (jnl_fence_ctl.level && next_fenced_was_null && actual_update && write_logical_recs)
				{	/* If ZTransaction and first KILL and the kill resulted in an update
					 * Note that "write_logical_recs" is used above instead of JNL_WRITE_LOGICAL_RECS(csa)
					 * since the value of the latter macro might have changed inside the call to t_end()
					 * (since jnl state changes could change the JNL_ENABLED check which is part of the macro).
					 */
					assert(NULL != csa->next_fenced);
					assert(jnl_fence_ctl.fence_list == csa);
					jnl_fence_ctl.fence_list = csa->next_fenced;
					csa->next_fenced = NULL;
				}
				need_kip_incr = FALSE;
				assert(!kip_incremented);
				continue;
			}
			/* In case this is MM and t_end caused a database extension, reset csd */
			assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
			csd = cs_data;
		} else
                {
                        cdb_status = tp_hist(alt_hist);
                        if (cdb_sc_normal != cdb_status)
                                goto retry;
                }
		INCR_GVSTATS_COUNTER(csa, csa->nl, n_kill, 1);
		if (0 != gv_target->clue.end)
		{	/* If clue is still valid, then the deletion was confined to a single block */
			assert(gv_target->hist.h[0].blk_num == alt_hist->h[0].blk_num);
			/* In this case, the "right hand" key (which was searched via gv_altkey) was the last search
			 * and should become the clue.  Furthermore, the curr.match from this last search should be
			 * the history's curr.match.  However, this record will have been shuffled to the position of
			 * the "left hand" key, and therefore, the original curr.offset should be left untouched. */
			gv_target->hist.h[0].curr_rec.match = alt_hist->h[0].curr_rec.match;
			memcpy(&gv_target->clue, gv_altkey, sizeof(gv_key) + gv_altkey->end);
		}
		if (0 == dollar_tlevel)
		{
			assert(0 < kill_set_head.used || !kip_incremented);
			if (0 < kill_set_head.used)     /* free subtree, decrease kill_in_prog */
			{	/* If csd->dsid is non-zero then some rc code was exercised before the changes
				 * to prevent pre-commit expansion of the kill subtree. Not clear on what to do now.
				 */
				assert(!csd->dsid);
                        	GTM_WHITE_BOX_TEST(WBTEST_ABANDONEDKILL, sleep_counter, SLEEP_ONE_MIN);
#				ifdef DEBUG
	                        if (SLEEP_ONE_MIN == sleep_counter)
				{
					assert(gtm_white_box_test_case_enabled);
					while (1 <= sleep_counter)
						wcs_sleep(sleep_counter--);
				}
#				endif
				gvcst_expand_free_subtree(&kill_set_head);
				/* In case this is MM and gvcst_expand_free_subtree() called gvcst_bmp_mark_free() called t_retry()
				 * which remapped an extended database, reset csd */
				assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
				csd = cs_data;
				DECR_KIP(csd, csa, kip_incremented);
			}
			assert(0 < kill_set_head.used || !kip_incremented);
			for (ks = kill_set_head.next_kill_set;  NULL != ks;  ks = temp_ks)
			{
				temp_ks = ks->next_kill_set;
				free(ks);
			}
			assert(0 < kill_set_head.used || !kip_incremented);
		}
		return;
retry:		t_retry(cdb_status);
		/* In case this is MM and t_retry() remapped an extended database, reset csd */
		assert((dba_mm == cs_data->acc_meth) || (csd == cs_data));
		csd = cs_data;
research:	;
	}
}
