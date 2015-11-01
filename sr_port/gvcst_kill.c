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

#include <stddef.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
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
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
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
#include "gvcst_search.h"
#include "gvcst_expand_free_subtree.h"
#include "gvcst_kill.h"
#include "rc_cpt_ops.h"
#include "add_inter.h"

error_def(ERR_GVKILLFAIL);

GBLREF	boolean_t		is_updproc;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF  boolean_t               pool_init;
GBLREF	char			*update_array, *update_array_ptr;
GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey, *gv_altkey;
GBLREF	int4			gv_keysize;
GBLREF	gv_namehead		*gv_target;
GBLREF	int			update_array_size;
GBLREF  int                     cumul_update_array_size;	/* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
GBLREF	jnl_fence_control	jnl_fence_ctl;
GBLREF	jnl_format_buffer	*non_tp_jfb_ptr;
GBLREF  kill_set                *kill_set_tail;
GBLREF	short			dollar_tlevel;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;
GBLREF	sgm_info		*sgm_info_ptr;
GBLREF	ua_list			*first_ua, *curr_ua;		/* needed for the ENSURE_UPDATE_ARRAY_SPACE macro */
GBLREF	uint4			t_err;
GBLREF	unsigned char		cw_set_depth;
GBLREF	unsigned int		t_tries;
GBLREF	boolean_t 		kip_incremented;
GBLREF	boolean_t		need_kip_incr;
GBLREF	boolean_t		is_replicator;
GBLREF jnl_gbls_t		jgbl;

void	gvcst_kill(bool do_subtree)
{
	bool			clue, flush_cache, left_extra, right_extra;
	boolean_t		actual_update, next_fenced_was_null, jnl_enabled;
	char			temp[4096], temp1[4096], *temp_ptr;
	cw_set_element		*tp_cse;
	enum cdb_sc		cdb_status;
	int			lev, segment_update_array_size;
	jnl_action		*ja;
	jnl_format_buffer	*jfb;
	kill_set		kill_set_head, *ks, *temp_ks;
	srch_hist		*alt_hist;
	srch_blk_status		*left,*right;
	srch_rec_status		*left_rec_stat, local_srch_rec;

	error_def(ERR_SCNDDBNOUPD);

	if ((FALSE == pool_init) && REPL_ENABLED(cs_data) && is_replicator)
		jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);
	if (REPL_ENABLED(cs_data) && pool_init && jnlpool_ctl->upd_disabled && !is_updproc)
		rts_error(VARLSTCNT(1) ERR_SCNDDBNOUPD);
	clue = 0 != gv_target->clue.end;
	if (0 == dollar_tlevel)
	{
		kill_set_head.next_kill_set = NULL;
		t_begin(ERR_GVKILLFAIL, TRUE);
		if (jnl_fence_ctl.level)	/* next_fenced_was_null is reliable only if we are in ZTransaction */
			next_fenced_was_null = (NULL == cs_addrs->next_fenced) ? TRUE : FALSE;
	} else if (NULL == sgm_info_ptr->first_cw_set)
			t_begin(ERR_GVKILLFAIL, TRUE);
	else
		t_err = ERR_GVKILLFAIL;
	assert(NULL != update_array);
	assert(NULL != update_array_ptr);
	assert(0 != update_array_size);
	assert(update_array + update_array_size >= update_array_ptr);
	for (;;)
	{
		assert(t_tries < CDB_STAGNATE || cs_addrs->now_crit);	/* we better hold crit in the final retry (TP & non-TP) */
		if (0 == dollar_tlevel)
		{
			update_array_ptr = update_array;
			kill_set_tail = &kill_set_head;
			for (ks = &kill_set_head;  NULL != ks;  ks = ks->next_kill_set)
				ks->used = 0;
		} else
		{
			segment_update_array_size = UA_NON_BM_SIZE(cs_data);
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
				cdb_status = gvcst_kill_blk(left->blk_num, lev, left->buffaddr, gv_currkey,
							*left_rec_stat, right->curr_rec, right_extra, &tp_cse);
				left->ptr = tp_cse;
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
				cdb_status = gvcst_kill_blk(left->blk_num, lev, left->buffaddr, gv_currkey,
							*left_rec_stat, local_srch_rec, FALSE, &tp_cse);
				left->ptr = tp_cse;
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
				cdb_status = gvcst_kill_blk(right->blk_num, lev, right->buffaddr, gv_altkey,
							local_srch_rec, right->curr_rec, right_extra, &tp_cse);
				right->ptr = tp_cse;
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
			actual_update = cw_set_depth;	/* for non-TP, tp_cse is NULL even if cw_set_depth is non-zero */
		} else
			assert(!actual_update || sgm_info_ptr->cw_set_depth);
		if ((jnl_enabled = JNL_ENABLED(cs_addrs)) && actual_update)
		{	/* Maintain journal records only if the kill actually resulted in an update. */
			if (0 == dollar_tlevel)
				jfb = non_tp_jfb_ptr; /* Already malloced in gvcst_init() */
			else
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
			if (0 == dollar_tlevel)
				jgbl.cumul_jnl_rec_len = jfb->record_size;
			else
				jgbl.cumul_jnl_rec_len += jfb->record_size;
			assert(0 == jgbl.cumul_jnl_rec_len % JNL_REC_START_BNDRY);
			DEBUG_ONLY(jgbl.cumul_index++;)
		}
		flush_cache = FALSE;
		if (0 == dollar_tlevel)
		{
			if ((0 != cs_data->dsid) && (0 < kill_set_head.used)
				&& gv_target->hist.h[1].blk_num != alt_hist->h[1].blk_num)
			{	/* multi-level delete */
				rc_cpt_inval();
				flush_cache = TRUE;
			}
			if (0 < kill_set_head.used)		/* increase kill_in_prog */
				need_kip_incr = TRUE;
			if (0 == t_end(&gv_target->hist, alt_hist))
			{
				if (jnl_fence_ctl.level && next_fenced_was_null && actual_update && jnl_enabled)
				{	/* If ZTransaction and first KILL and the kill resulted in an update
					 * Note that "jnl_enabled" is used above instead of JNL_ENABLED(cs_addrs) since the
					 * 	latter might have changed inside the call to t_end() above.
					 */
					assert(NULL != cs_addrs->next_fenced);
					assert(jnl_fence_ctl.fence_list == cs_addrs);
					jnl_fence_ctl.fence_list = cs_addrs->next_fenced;
					cs_addrs->next_fenced = NULL;
				}
				need_kip_incr = FALSE;
				assert(!kip_incremented);
				continue;
			}
		} else
                {
                        cdb_status = tp_hist(alt_hist);
                        if (cdb_sc_normal != cdb_status)
                                goto retry;
                }
		++cs_data->n_kills;
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
			{	/* If cs_data ->dsid is non-zero then some rc code was exercised before the changes
				 * to prevent pre-commit expansion of the kill subtree. Not clear on what to do now.
				 */
				assert(!cs_data->dsid);
				gvcst_expand_free_subtree(&kill_set_head);
				DECR_KIP(cs_data, cs_addrs, kip_incremented);
			}
			for (ks = kill_set_head.next_kill_set;  NULL != ks;  ks = temp_ks)
			{
				temp_ks = ks->next_kill_set;
				free(ks);
			}
		}
		return;
retry:		t_retry(cdb_status);
research:	;
	}
}
