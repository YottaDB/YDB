/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#if defined(UNIX)
#include "gtm_unistd.h"
#include "eintr_wrappers.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif

#include "gdsroot.h"
#include "gtm_rename.h"
#include "gdsblk.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gtmio.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"
#include "jnl_typedef.h"
#include "gtmmsg.h"
#include "anticipatory_freeze.h"
#include "wcs_flu.h"	/* for wcs_flu() prototype */
#include "wbox_test_init.h"

GBLREF	reg_ctl_list		*mur_ctl;
GBLREF	mur_gbls_t		murgbl;
GBLREF	jnl_gbls_t		jgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	sgmnt_data_ptr_t	cs_data;

error_def(ERR_PREMATEOF); /* for JNL_DO_FILE_WRITE */
error_def(ERR_JNLNOCREATE);
error_def(ERR_JNLWRERR);
error_def(ERR_JNLFSYNCERR);
error_def(ERR_TEXT);

uint4 mur_process_intrpt_recov()
{
	jnl_ctl_list			*jctl, *last_jctl;
	reg_ctl_list			*rctl, *rctl_top;
	int				rename_fn_len, save_name_len, idx;
	char				prev_jnl_fn[MAX_FN_LEN + 1], rename_fn[MAX_FN_LEN + 1], save_name[MAX_FN_LEN + 1];
	jnl_create_info			jnl_info;
	uint4				status, status2;
	uint4				max_autoswitchlimit, max_jnl_alq, max_jnl_deq, freeblks;
	sgmnt_data_ptr_t		csd;
	UNIX_ONLY(
		jnl_private_control	*jpc;
		jnl_buffer_ptr_t	jbp;
	)
	VMS_ONLY(
		io_status_block_disk	iosb;
	)
	boolean_t			jfh_changed;
	jnl_record			*jnlrec;
	jnl_file_header			*jfh;
	jnl_tm_t			now;

	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		TP_CHANGE_REG(rctl->gd);
		csd = cs_data;	/* MM logic after wcs_flu call requires this to be set */
		assert(csd == rctl->csa->hdr);
		jctl = rctl->jctl_turn_around;
		max_jnl_alq = max_jnl_deq = max_autoswitchlimit = 0;
		for (last_jctl = NULL ; (NULL != jctl); last_jctl = jctl, jctl = jctl->next_gen)
		{
			jfh = jctl->jfh;
			if (max_autoswitchlimit < jfh->autoswitchlimit)
			{	/* Note that max_jnl_alq, max_jnl_deq are not the maximum journal allocation/extensions across
				 * generations, but rather the allocation/extension corresponding to the maximum autoswitchlimit.
				 */
				max_autoswitchlimit = jfh->autoswitchlimit;
				max_jnl_alq         = jfh->jnl_alq;
				max_jnl_deq         = jfh->jnl_deq;
			}
			/* Until now, "rctl->blks_to_upgrd_adjust" holds the number of V4 format newly created bitmap blocks
			 * seen in INCTN records in backward processing. It is possible that backward processing might have
			 * missed out on seeing those INCTN records which are part of virtually-truncated or completely-rolled-bak
			 * journal files. The journal file-header has a separate field "prev_recov_blks_to_upgrd_adjust" which
			 * maintains exactly this count. Therefore adjust the rctl counter accordingly.
			 */
			assert(!jfh->prev_recov_blks_to_upgrd_adjust || !jfh->recover_interrupted);
			assert(!jfh->prev_recov_blks_to_upgrd_adjust || jfh->prev_recov_end_of_data);
			rctl->blks_to_upgrd_adjust += jfh->prev_recov_blks_to_upgrd_adjust;
		}
		if (max_autoswitchlimit > last_jctl->jfh->autoswitchlimit)
		{
			csd->jnl_alq         = max_jnl_alq;
			csd->jnl_deq         = max_jnl_deq;
			csd->autoswitchlimit = max_autoswitchlimit;
		} else
		{
			assert(csd->jnl_alq         == last_jctl->jfh->jnl_alq);
			assert(csd->jnl_deq         == last_jctl->jfh->jnl_deq);
			assert(csd->autoswitchlimit == last_jctl->jfh->autoswitchlimit);
		}
		jctl = rctl->jctl_turn_around;
		/* Get a pointer to the turn around point EPOCH record */
		jnlrec = rctl->mur_desc->jnlrec;
		assert(JRT_EPOCH == jnlrec->prefix.jrec_type);
		assert(jctl->turn_around_time == jnlrec->prefix.time);
		assert(jctl->turn_around_seqno == jnlrec->jrec_epoch.jnl_seqno);
		assert(jctl->turn_around_tn == jnlrec->prefix.tn);
		assert(jctl->rec_offset == jctl->turn_around_offset);
		/* Reset file-header "blks_to_upgrd" counter to the turn around point epoch value. Adjust this to include
		 * the number of new V4 format bitmaps created by post-turnaround-point db file extensions.
		 * The adjustment value is maintained in rctl->blks_to_upgrd_adjust.
		 */
		csd->blks_to_upgrd = jnlrec->jrec_epoch.blks_to_upgrd;
		csd->blks_to_upgrd += rctl->blks_to_upgrd_adjust;
#		ifdef GTM_TRIGGER
		/* online rollback can potentially take the database to a point in the past where the triggers that were
		 * previously installed are no longer a part of the current database state and so any process that restarts
		 * AFTER online rollback completes SHOULD reload triggers and the only way to do that is by incrementing the
		 * db_trigger_cycle in the file header.
		 */
		if (jgbl.onlnrlbk && (0 < csd->db_trigger_cycle))
		{	/* check for non-zero db_trigger_cycle is to prevent other processes (continuing after online rollback)
			 * to establish implicit TP (on seeing the trigger cycle mismatch) when there are actually no triggers
			 * installed in the database (because there were none at the start of online rollback).
			 */
			csd->db_trigger_cycle++;
			if (0 == csd->db_trigger_cycle)
				csd->db_trigger_cycle = 1;	/* Don't allow cycle set to 0 which means uninitialized */
		}
#		endif
		assert((WBTEST_ALLOW_ARBITRARY_FULLY_UPGRADED == gtm_white_box_test_case_number) ||
			(FALSE == jctl->turn_around_fullyupgraded) || (TRUE == jctl->turn_around_fullyupgraded));
		/* Set csd->fully_upgraded to FALSE if:
		 * a) The turn around EPOCH had the fully_upgraded field set to FALSE
		 * OR
		 * b) If csd->blks_to_upgrd counter is non-zero. This field can be non-zero even if the turnaround EPOCH's
		 * fully_upgraded field is TRUE. This is possible if the database was downgraded to V4 (post turnaround EPOCH)
		 * format and database extensions happened causing new V4 format bitmap blocks to be written. The count of V4
		 * format bitmap blocks is maintained ONLY as part of INCTN records (with INCTN opcode SET_JNL_FILE_CLOSE_EXTEND)
		 * noted down in rctl->blks_to_upgrd_adjust counter as part of BACKWARD processing which are finally added to
		 * csd->blks_to_upgrd.
		 */
		if (!jctl->turn_around_fullyupgraded || csd->blks_to_upgrd)
			csd->fully_upgraded = FALSE;
		csd->trans_hist.early_tn = jctl->turn_around_tn;
		csd->trans_hist.curr_tn = csd->trans_hist.early_tn;	/* INCREMENT_CURR_TN macro not used but noted in comment
									 * to identify all places that set curr_tn */
		csd->jnl_eovtn = csd->trans_hist.curr_tn;
		csd->turn_around_point = TRUE;
		/* MUPIP REORG UPGRADE/DOWNGRADE stores its partially processed state in the database file header.
		 * It is difficult for recovery to restore those fields to a correct partial value.
		 * Hence reset the related fields as if the desired_db_format got set just ONE tn BEFORE the EPOCH record
		 * 	and that there was no more processing that happened.
		 * This might potentially mean some duplicate processing for MUPIP REORG UPGRADE/DOWNGRADE after the recovery.
		 * But that will only be the case as long as the database is in compatibility (mixed) mode (hopefully not long).
		 */
		if (csd->desired_db_format_tn >= jctl->turn_around_tn)
			csd->desired_db_format_tn = jctl->turn_around_tn - 1;
		if (csd->reorg_db_fmt_start_tn >= jctl->turn_around_tn)
			csd->reorg_db_fmt_start_tn = jctl->turn_around_tn - 1;
		if (csd->tn_upgrd_blks_0 > jctl->turn_around_tn)
			csd->tn_upgrd_blks_0 = (trans_num)-1;
		csd->reorg_upgrd_dwngrd_restart_block = 0;
		/* Compute current value of "free_blocks" based on the value of "free_blocks" at the turnaround point epoch
		 * record and the change in "total_blks" since that epoch to the present form of the database. Any difference
		 * in "total_blks" implies database file extensions happened since the turnaround point. A backward rollback
		 * undoes everything (including all updates) except file extensions (it does not truncate the file size).
		 * Therefore every block that was newly allocated as part of those file extensions should be considered FREE
		 * for the current calculations except for the local bitmap blocks which are BUSY the moment they are created.
		 */
		assert(jnlrec->jrec_epoch.total_blks <= csd->trans_hist.total_blks);
		csd->trans_hist.free_blocks = jnlrec->jrec_epoch.free_blocks
			+ (csd->trans_hist.total_blks - jnlrec->jrec_epoch.total_blks)
			- DIVIDE_ROUND_UP(csd->trans_hist.total_blks, BLKS_PER_LMAP)
			+ DIVIDE_ROUND_UP(jnlrec->jrec_epoch.total_blks, BLKS_PER_LMAP);
		assert(!csd->blks_to_upgrd || !csd->fully_upgraded);
		assert((freeblks = mur_blocks_free(rctl)) == csd->trans_hist.free_blocks);
		/* Update strm_reg_seqno[] in db file header to reflect the turn around point.
		 * Before updating "strm_reg_seqno", make sure value is saved into "save_strm_reg_seqno".
		 * This is relied upon by the function "mur_get_max_strm_reg_seqno" in case of interrupted rollback.
		 */
		for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
		{
			if (!csd->save_strm_reg_seqno[idx])
				csd->save_strm_reg_seqno[idx] = csd->strm_reg_seqno[idx];
			csd->strm_reg_seqno[idx] = jnlrec->jrec_epoch.strm_seqno[idx];
		}
		wcs_flu(WCSFLU_FLUSH_HDR | WCSFLU_FSYNC_DB);
		assert(cs_addrs->ti->curr_tn == jctl->turn_around_tn);
#		ifdef UNIX
		if (jgbl.onlnrlbk)
		{
			if (dba_bg == cs_addrs->hdr->acc_meth)
			{	/* dryclean the cache (basically reset the cycle fields in all teh cache records) so as to make
				 * GT.M processes that only does 'reads' to require crit and hence realize that online rollback
				 * is in progress
				 */
				bt_refresh(cs_addrs, FALSE); /* sets earliest bt TN to be the turn around TN */
			}
			db_csh_ref(cs_addrs, FALSE);
			assert(NULL != cs_addrs->jnl);
			jpc = cs_addrs->jnl;
			assert(NULL != jpc->jnl_buff);
			jbp = jpc->jnl_buff;
			/* Since Rollback simulates the journal record along with the timestamp at which the update was made, it
			 * sets jgbl.dont_reset_gbl_jrec_time to TRUE so that during forward processing t_end or tp_tend does not
			 * reset the gbl_jrec_time to reflect the current time. But, with Online Rollback, one can have the shared
			 * memory up and running and hence can have jbp->prev_jrec_time to be the time of the most recent journal
			 * update made. Later in t_end/tp_tend, ADJUST_GBL_JREC_TIME is invoked which ensures that if ever
			 * gbl_jrec_time (the time of the current update) is less than jbp->prev_jrec_time (time of the latest
			 * journal update), dont_reset_gbl_jrec_time better be FALSE. But, this assert will trip since Rollback
			 * sets the latter to TRUE. To fix this, set jbp->prev_jrec_time to the turn around time stamp. This way
			 * we are guaranteed that all the updates done in the forward processing will have a timestamp that is
			 * greater than the turn around timestamp
			 */
			jbp->prev_jrec_time = jctl->turn_around_time;
		} else if (dba_bg == csd->acc_meth)
		{	/* set earliest bt TN to be the turn-around TN (taken from bt_refresh()) */
			SET_OLDEST_HIST_TN(cs_addrs, cs_addrs->ti->curr_tn - 1);
		}
#		else
		if (dba_bg == csd->acc_meth)
		{	/* set earliest bt TN to be the turn-around TN (taken from bt_refresh()) */
			SET_OLDEST_HIST_TN(cs_addrs, cs_addrs->ti->curr_tn - 1);
		}
#		endif
		csd->turn_around_point = FALSE;
		assert(OLDEST_HIST_TN(cs_addrs) == (cs_addrs->ti->curr_tn - 1));
		/* In case this is MM and wcs_flu() remapped an extended database, reset rctl->csd */
		assert((dba_mm == cs_data->acc_meth) || (rctl->csd == cs_data));
		rctl->csd = cs_data;
	}
	JNL_SHORT_TIME(now);
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
	{
		TP_CHANGE_REG_IF_NEEDED(rctl->gd);
		if (!rctl->jfh_recov_interrupted)
			jctl = rctl->jctl_turn_around;
		else
		{
			DEBUG_ONLY(
				for (jctl = rctl->jctl_turn_around; NULL != jctl->next_gen; jctl = jctl->next_gen)
					;
				/* check that latest gener file name does not match db header */
				assert((rctl->csd->jnl_file_len != jctl->jnl_fn_len)
					|| (0 != memcmp(rctl->csd->jnl_file_name, jctl->jnl_fn, jctl->jnl_fn_len)));
			)
			jctl = rctl->jctl_alt_head;
		}
		assert(NULL != jctl);
		for ( ; NULL != jctl->next_gen; jctl = jctl->next_gen)
			;
		assert(rctl->csd->jnl_file_len == jctl->jnl_fn_len); 			       /* latest gener file name */
		assert(0 == memcmp(rctl->csd->jnl_file_name, jctl->jnl_fn, jctl->jnl_fn_len)); /* should match db header */
		if (SS_NORMAL != (status = prepare_unique_name((char *)jctl->jnl_fn, jctl->jnl_fn_len, "", "",
								rename_fn, &rename_fn_len, now, &status2)))
			return status;
		jctl->jnl_fn_len = rename_fn_len;  /* change the name in memory to the proposed name */
		memcpy(jctl->jnl_fn, rename_fn, rename_fn_len + 1);
		/* Rename hasn't happened yet at the filesystem level. In case current recover command is interrupted,
		 * we need to update jfh->next_jnl_file_name before mur_forward(). Update jfh->next_jnl_file_name for
		 * all journal files from which PBLK records were applied. Create new journal files for forward play.
		 */
		assert(NULL != rctl->jctl_turn_around);
		jctl = rctl->jctl_turn_around; /* points to journal file which has current recover's turn around point */
		assert(0 != jctl->turn_around_offset);
		jfh = jctl->jfh;
		jfh->turn_around_offset = jctl->turn_around_offset;	/* save progress in file header for 	*/
		jfh->turn_around_time = jctl->turn_around_time;		/* possible re-issue of recover 	*/
		UNIX_ONLY(
			for (idx = 0; idx < MAX_SUPPL_STRMS; idx++)
				jfh->strm_end_seqno[idx] = csd->strm_reg_seqno[idx];
		)
		jfh_changed = TRUE;
		for ( ; NULL != jctl; jctl = jctl->next_gen)
		{	/* setup the next_jnl links. note that in the case of interrupted recovery, next_jnl links
			 * would have been already set starting from the turn-around point journal file of the
			 * interrupted recovery but the new recovery MIGHT have taken us to a still previous
			 * generation journal file that needs its next_jnl link set. this is why we do the next_jnl
			 * link setup even in the case of interrupted recovery although in most cases it is unnecessary.
			 */
			jfh = jctl->jfh;
			if (NULL != jctl->next_gen)
			{
				jfh->next_jnl_file_name_length = jctl->next_gen->jnl_fn_len;
				memcpy(jfh->next_jnl_file_name, jctl->next_gen->jnl_fn, jctl->next_gen->jnl_fn_len);
				jfh_changed = TRUE;
			} else
				assert(0 == jfh->next_jnl_file_name_length); /* null link from latest generation */
			if (jfh->turn_around_offset && (jctl != rctl->jctl_turn_around))
			{	/* It is possible that the current recovery has a turn-around-point much before the
				 * previously interrupted recovery. If it happens to be a previous generation journal
				 * file then we have to reset the original turn-around-point to be zero in the journal
				 * file header in order to ensure if this recovery gets interrupted we do interrupted
				 * recovery processing until the new turn-around-point instead of stopping incorrectly
				 * at the original turn-around-point itself. Note that there could be more than one
				 * journal file with a non-zero turn_around_offset (depending on how many previous
				 * recoveries got interrupted in this loop) that need to be reset.
				 */
				assert(!jctl->turn_around_offset);
				assert(rctl->recov_interrupted);	/* rctl->jfh_recov_interrupted can fail */
				jfh->turn_around_offset = 0;
				jfh->turn_around_time = 0;
				jfh_changed = TRUE;
			}
			if (jfh_changed)
			{
				/* Since overwriting the journal file header (an already allocated block
				 * in the file) should not cause ENOSPC, we dont take the trouble of
				 * passing csa or jnl_fn (first two parameters). Instead we pass NULL.
				 */
				JNL_DO_FILE_WRITE(NULL, NULL, jctl->channel, 0, jfh,
					REAL_JNL_HDR_LEN, jctl->status, jctl->status2);
				if (SS_NORMAL != jctl->status)
				{
					assert(FALSE);
					if (SS_NORMAL == jctl->status2)
						gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(5) ERR_JNLWRERR, 2, jctl->jnl_fn_len,
							jctl->jnl_fn, jctl->status);
					else
						gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT1(6) ERR_JNLWRERR, 2, jctl->jnl_fn_len,
							jctl->jnl_fn, jctl->status, PUT_SYS_ERRNO(jctl->status2));
					return jctl->status;
				}
				UNIX_ONLY(
					GTM_JNL_FSYNC(rctl->csa, jctl->channel, jctl->status);
					if (-1 == jctl->status)
					{
						jctl->status2 = errno;
						assert(FALSE);
						gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(9) ERR_JNLFSYNCERR, 2,
							jctl->jnl_fn_len, jctl->jnl_fn,
							ERR_TEXT, 2, RTS_ERROR_TEXT("Error with fsync"), jctl->status2);
						return ERR_JNLFSYNCERR;
					}
				)
			}
			jfh_changed = FALSE;
		}
		memset(&jnl_info, 0, SIZEOF(jnl_info));
		jnl_info.status = jnl_info.status2 = SS_NORMAL;
		jnl_info.prev_jnl = &prev_jnl_fn[0];
		set_jnl_info(rctl->gd, &jnl_info);
		jnl_info.prev_jnl_len = rctl->jctl_turn_around->jnl_fn_len;
		memcpy(jnl_info.prev_jnl, rctl->jctl_turn_around->jnl_fn, rctl->jctl_turn_around->jnl_fn_len);
		jnl_info.prev_jnl[jnl_info.prev_jnl_len] = 0;
		jnl_info.jnl_len = rctl->csd->jnl_file_len;
		memcpy(jnl_info.jnl, rctl->csd->jnl_file_name, jnl_info.jnl_len);
		jnl_info.jnl[jnl_info.jnl_len] = 0;
		assert(!mur_options.rollback || jgbl.mur_rollback);
		jnl_info.reg_seqno = rctl->jctl_turn_around->turn_around_seqno;
		jgbl.gbl_jrec_time = rctl->jctl_turn_around->turn_around_time;	/* time needed for cre_jnl_file_common() */
		if (EXIT_NRM != cre_jnl_file_common(&jnl_info, rename_fn, rename_fn_len))
		{
			gtm_putmsg_csa(CSA_ARG(rctl->csa) VARLSTCNT(4) ERR_JNLNOCREATE, 2, jnl_info.jnl_len, jnl_info.jnl);
			return jnl_info.status;
		}
#		ifdef UNIX
		if (jgbl.onlnrlbk)
		{
			cs_addrs = rctl->csa;
			/* Mimic what jnl_file_close in case of cleanly a closed journal file */
			jpc = cs_addrs->jnl; /* the previous loop makes sure cs_addrs->jnl->jnl_buff is valid*/
			NULLIFY_JNL_FILE_ID(cs_addrs);
			jpc->jnl_buff->cycle++; /* so that, all other processes knows to switch to newer journal file */
			jpc->cycle--; /* decrement cycle so jnl_ensure_open() knows to reopen the journal */
		}
#		endif
		if (NULL != rctl->jctl_alt_head) /* remove the journal files created by last interrupted recover process */
		{
			mur_rem_jctls(rctl);
			rctl->jctl_alt_head = NULL;
		}
		/* From this point on, journal records are written into the newly created journal file. However, we still read
		 * from old journal files.
		 */
	}
	jgbl.gbl_jrec_time = 0;	/* set to a safe value */
	return SS_NORMAL;
}
