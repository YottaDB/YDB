/****************************************************************
 *								*
 *	Copyright 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtmio.h"
#include "gtm_string.h"
#include "gtm_time.h"
#if defined(UNIX)
#include "gtm_unistd.h"
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
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "muprec.h"
#include "iosp.h"
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "gtmmsg.h"
#include "file_head_read.h"
#include "file_head_write.h"
#if defined(UNIX)
#include "mu_rndwn_replpool.h"
#include "ftok_sems.h"
#endif
#include "util.h"

#define WARN_STATUS(jctl)									\
if (SS_NORMAL != jctl->status)									\
{												\
	if (SS_NORMAL != jctl->status2)								\
		gtm_putmsg(VARLSTCNT1(6) ERR_JNLWRERR, 2, jctl->jnl_fn_len, jctl->jnl_fn,	\
			jctl->status, PUT_SYS_ERRNO(jctl->status2));				\
	else											\
		gtm_putmsg(VARLSTCNT(5) ERR_JNLWRERR,						\
			2, jctl->jnl_fn_len, jctl->jnl_fn, jctl->status);			\
	wrn_count++;										\
}												\

GBLREF	void		(*call_on_signal)();
GBLREF	jnl_gbls_t	jgbl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	mur_gbls_t	murgbl;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	char		*jnl_state_lit[];
GBLREF	char		*repl_state_lit[];

void	mur_close_files(void)
{
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		jctl_temp, *jctl, *prev_jctl, *end_jctl;
	sgmnt_data_ptr_t	csd;
	sgmnt_data		csd_temp;
	int			head_jnl_fn_len, wrn_count = 0;
	uint4			ustatus;
	int4			status;
	char 			*head_jnl_fn, *rename_fn, fn[MAX_FN_LEN];
	int 			rename_fn_len;
	boolean_t		set_resync_to_region = FALSE;
	file_control		*fc;
#if defined(VMS)
	vms_gds_info		*gds_info;
	io_status_block_disk	iosb;
	short			channel;
#elif defined(UNIX)
	int			channel;
#endif

	error_def(ERR_PREMATEOF);
	error_def(ERR_JNLSTRESTFL);
	error_def(ERR_SETREG2RESYNC);
	error_def(ERR_JNLSTATE);
	error_def(ERR_REPLSTATE);
	error_def(ERR_FILERENAME);
	error_def(ERR_RENAMEFAIL);
	error_def(ERR_JNLREAD);
	error_def(ERR_JNLBADLABEL);
	error_def(ERR_JNLWRERR);
	error_def(ERR_MUJNLSTAT);
	error_def(ERR_JNLSUCCESS);
	error_def(ERR_JNLACTINCMPLT);

	call_on_signal = NULL;	/* Do not recurs via call_on_signal if there is an error */
	csd = &csd_temp;
	JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* For jnl_write* routnies we need this */
	for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_full_total; rctl < rctl_top; rctl++)
	{
		if (NULL != rctl->csa)
		{ 	/* gvcst_init was called */
			gv_cur_region = rctl->gd;
			tp_change_reg();
			if (mur_options.update && JNL_ENABLED(rctl))
				cs_addrs->jnl->pini_addr = 0; /* Stop simulation of GTM process journal record writing */
			if (NULL != rctl->jctl && murgbl.clean_exit && mur_options.rollback)
			{	/* to write proper jnl_seqno in epoch record */
				assert(murgbl.stop_rlbk_seqno >= murgbl.resync_seqno);
				assert(murgbl.stop_rlbk_seqno >= murgbl.consist_jnl_seqno);
				if (murgbl.consist_jnl_seqno) /* can be zero if this command is a no-operation */
					jgbl.mur_jrec_seqno = cs_addrs->hdr->reg_seqno = murgbl.consist_jnl_seqno;
				if (rctl->jctl->jfh->crash && rctl->jctl->jfh->update_disabled)
					/* Set resync_to_region seqno for a crash and update_disable case */
					set_resync_to_region = TRUE;
			}
			assert(NULL != rctl->csa->nl);
			assert(!mur_options.update || rctl->csa->nl->donotflush_dbjnl);
			assert(mur_options.update || !rctl->csa->nl->donotflush_dbjnl);
			if (mur_options.update && (murgbl.clean_exit || !murgbl.db_updated) && (NULL != rctl->csa->nl))
				rctl->csa->nl->donotflush_dbjnl = FALSE;	/* shared memory is now clean for dbjnl flushing */
			gds_rundown();
			if (rctl->standalone && (murgbl.clean_exit || !murgbl.db_updated) &&
					!rctl->gd->read_only && file_head_read((char *)rctl->gd->dyn.addr->fname, csd))
			{
				assert(mur_options.update);
				csd->file_corrupt = FALSE;
				if (murgbl.clean_exit)
				{
					if (mur_options.rollback)
						csd->repl_state = rctl->repl_state;
					/* After recover replication state is always closed */
					if (rctl->repl_state != csd->repl_state)
						gtm_putmsg(VARLSTCNT(8) ERR_REPLSTATE, 6, LEN_AND_LIT(FILE_STR),
							DB_LEN_STR(gv_cur_region),
							LEN_AND_STR(repl_state_lit[csd->repl_state]));
					if (rctl->jnl_state != csd->jnl_state)
						gtm_putmsg(VARLSTCNT(8) ERR_JNLSTATE, 6, LEN_AND_LIT(FILE_STR),
							DB_LEN_STR(gv_cur_region),
							LEN_AND_STR(jnl_state_lit[csd->jnl_state]));
					if (NULL != rctl->jctl && mur_options.rollback && murgbl.consist_jnl_seqno)
					{
						if (set_resync_to_region)
						{
							csd->resync_seqno = csd->reg_seqno;
							if (mur_options.verbose)
								gtm_putmsg(VARLSTCNT(6) ERR_SETREG2RESYNC, 4,
								&csd->resync_seqno, &csd->reg_seqno, DB_LEN_STR(rctl->gd));
						}
						csd->reg_seqno = murgbl.consist_jnl_seqno;
						if (csd->resync_seqno > murgbl.consist_jnl_seqno)
							csd->resync_seqno = murgbl.consist_jnl_seqno;
					}
					csd->intrpt_recov_resync_seqno = 0;
					csd->intrpt_recov_tp_resolve_time = 0;
					csd->intrpt_recov_jnl_state = jnl_notallowed;
					csd->intrpt_recov_repl_state = repl_closed;
					csd->recov_interrupted = FALSE;
				} else
				{	/* Always restore states. Otherwise, reissuing the command might fail */
					csd->repl_state = rctl->repl_state;
					csd->jnl_state = rctl->jnl_state;
					csd->jnl_before_image = rctl->before_image;
					csd->recov_interrupted = rctl->recov_interrupted;
				}
				if (!file_head_write((char *)rctl->gd->dyn.addr->fname, csd))
					wrn_count++;
			} /* else do not restore state */
			if (rctl->standalone && !mur_options.forward && murgbl.clean_exit && (NULL != rctl->jctl_turn_around))
			{	/* some backward processing and possibly forward processing was done. do some cleanup */
				assert(NULL == rctl->jctl_turn_around || NULL != rctl->jctl_head);
				jctl = rctl->jctl_turn_around;
				head_jnl_fn_len = jctl->jnl_fn_len;
				head_jnl_fn = fn;
				memcpy(head_jnl_fn, jctl->jnl_fn, head_jnl_fn_len);
				/* reset jctl->jfh->recover_interrupted field in all recover created jnl files to signal
				 * that a future recover should not consider this recover as an interrupted recover.
				 */
				jctl = &jctl_temp;
				memset(&jctl_temp, 0, sizeof(jctl_temp));
				jctl->jnl_fn_len = csd->jnl_file_len;
				memcpy(jctl->jnl_fn, csd->jnl_file_name, jctl->jnl_fn_len);
				jctl->jnl_fn[jctl->jnl_fn_len] = 0;
				while (0 != jctl->jnl_fn_len)
				{
					if (jctl->jnl_fn_len == head_jnl_fn_len &&
						0 == memcmp(jctl->jnl_fn, head_jnl_fn, jctl->jnl_fn_len))
						break;
					if (!mur_fopen(jctl))
					{	/* if opening the journal file failed, we cannot do anything here */
						wrn_count++;
						/* mur_fopen() would have done the appropriate gtm_putmsg() */
						break;
					}
					/* at this point jctl->jfh->recover_interrupted is expected to be TRUE
					 * except in a few cases like mur_back_process() encountered an error in
					 * mur_insert_prev() because of missing journal.
					 * in that case we would not have gone through mur_process_intrpt_recov()
					 * so we would not have created new journal files.
					 */
					if (jctl->jfh->recover_interrupted)
					{
						jctl->jfh->recover_interrupted = FALSE;
						DO_FILE_WRITE(jctl->channel, 0, jctl->jfh, JNL_HDR_LEN,
							jctl->status, jctl->status2);
						WARN_STATUS(jctl);
					}
					jctl->jnl_fn_len = jctl->jfh->prev_jnl_file_name_length;
					memcpy(jctl->jnl_fn, jctl->jfh->prev_jnl_file_name, jctl->jnl_fn_len);
					jctl->jnl_fn[jctl->jnl_fn_len] = 0;
					if (!mur_fclose(jctl))
						/* mur_fclose() would have done the appropriate gtm_putmsg() */
						wrn_count++;
				}
				jctl = rctl->jctl_turn_around;
				assert(!jctl->jfh->recover_interrupted);
				/* reset fields in turn-around-point journal file header to
				 * reflect new virtually truncated journal file */
				assert(jctl->turn_around_offset);
				jctl->jfh->turn_around_offset = 0;
				jctl->jfh->turn_around_time = 0;
				jctl->jfh->crash = 0;
				jctl->jfh->end_of_data = jctl->turn_around_offset;
				jctl->jfh->eov_timestamp = jctl->turn_around_time;
				jctl->jfh->eov_tn = jctl->turn_around_tn;
				if (mur_options.rollback)
					jctl->jfh->end_seqno = jctl->turn_around_seqno;
				assert(0 == jctl->jfh->prev_recov_end_of_data ||
					jctl->jfh->prev_recov_end_of_data >= jctl->lvrec_off);
				if (0 == jctl->jfh->prev_recov_end_of_data)
					jctl->jfh->prev_recov_end_of_data = jctl->lvrec_off;
				jctl->jfh->next_jnl_file_name_length = 0;
				DO_FILE_WRITE(jctl->channel, 0, jctl->jfh, JNL_HDR_LEN, jctl->status, jctl->status2);
				WARN_STATUS(jctl);
				/* we have to clear next_jnl_file_name fields in the post-turn-around-point journal files.
				 * but if we get killed in this process, a future recover should be able to resume
				 * the cleanup.  since a future recover can only start from the turn-around-point
				 * journal file and follow the next chains, it is important that we remove the next
				 * chain from the end rather than from the beginning.
				 */
				for (end_jctl = jctl; NULL != end_jctl->next_gen; )	/* find the latest gener */
					end_jctl = end_jctl->next_gen;
				for ( ; end_jctl != jctl; end_jctl = end_jctl->prev_gen)
				{	/* Clear next_jnl_file_name fields in the post-turn-around-point journal files */
					assert(0 == end_jctl->turn_around_offset);
					end_jctl->jfh->next_jnl_file_name_length = 0;
					DO_FILE_WRITE(end_jctl->channel, 0, end_jctl->jfh, JNL_HDR_LEN,
						end_jctl->status, end_jctl->status2);
					WARN_STATUS(end_jctl);
					/* Rename journals whose entire contents have been undone with
					 * the rolled_bak prefix. user can decide to delete these */
					rename_fn = fn;
					prepare_unique_name((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
						PREFIX_ROLLED_BAK, "", rename_fn, &rename_fn_len, &ustatus);
					if (SS_NORMAL == gtm_rename((char *)end_jctl->jnl_fn, end_jctl->jnl_fn_len,
									rename_fn, rename_fn_len, &ustatus))
					{
						gtm_putmsg(VARLSTCNT (6) ERR_FILERENAME, 4, end_jctl->jnl_fn_len,
							end_jctl->jnl_fn, rename_fn_len, rename_fn);
					} else
					{
						gtm_putmsg(VARLSTCNT(6) ERR_RENAMEFAIL, 4,
							end_jctl->jnl_fn_len, end_jctl->jnl_fn, rename_fn_len, rename_fn);
						wrn_count++;
					}
				} /* end for */
			}
			rctl->csa = NULL;
		} /* end if (NULL != rctl->csa) */
		for (jctl = rctl->jctl_head; NULL != jctl; )
		{	/* NULL value of jctl_head possible if we errored out in mur_open_files() before constructing jctl list */
			prev_jctl = jctl;
			jctl = jctl->next_gen;
			if (!mur_fclose(prev_jctl))
				wrn_count++;	/* mur_fclose() would have done the appropriate gtm_putmsg() */
		}
		rctl->jctl_head = NULL;	/* So that we do not come to above loop again */
		UNIX_ONLY(
			if (rctl->standalone && !db_ipcs_reset(rctl->gd, !murgbl.clean_exit))
				wrn_count++;
			rctl->standalone = FALSE;
		)
		rctl->gd = NULL;
	}
	mur_close_file_extfmt();
#if defined(UNIX)
	if (mur_options.rollback && murgbl.repl_standalone)
	{
		mu_replpool_remove_sem(FALSE);
		murgbl.repl_standalone = FALSE;
	}
#endif
	if (wrn_count)
		gtm_putmsg(VARLSTCNT (1) ERR_JNLACTINCMPLT);
	else if (murgbl.clean_exit && !murgbl.wrn_count)
		JNL_SUCCESS_MSG(mur_options);
 	JNL_PUT_MSG_PROGRESS("End processing");
}
