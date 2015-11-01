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

#include "gdsroot.h"
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
#include "gtmmsg.h"
#include "dbfilop.h"
#if defined(UNIX)
#include "gtm_unistd.h"
#include "gdsbgtr.h"
GBLREF	gd_region		*gv_cur_region;
GBLREF	volatile int4		db_fsync_in_prog;	/* for DB_FSYNC macro usage */
#endif
GBLREF int			mur_regno;
GBLREF reg_ctl_list		*mur_ctl;
GBLREF jnl_ctl_list		*mur_jctl;
GBLREF mur_gbls_t		murgbl;
GBLREF mur_opt_struct 		mur_options;
GBLREF mur_rab_t		mur_rab;
GBLREF seq_num			seq_num_zero;

uint4 mur_apply_pblk(boolean_t apply_intrpt_pblk)
{
	uint4			status;
	reg_ctl_list		*rctl, *rctl_top;
	jnl_ctl_list		*tmpjctl;
	file_control		*fc;
        UNIX_ONLY(unix_db_info   *udi;)

	error_def(ERR_JNLREADBOF);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_NOPREVLINK);
	error_def(ERR_MUJNINFO);

	murgbl.db_updated = TRUE;
	for (mur_regno = 0, rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++, mur_regno++)
	{
		if (!apply_intrpt_pblk)
		{
			assert(NULL != rctl->jctl_turn_around);
			if (!rctl->jfh_recov_interrupted)
			{
				if (mur_options.verify)
				{
					mur_jctl = rctl->jctl;
					while (NULL != mur_jctl->next_gen)
						mur_jctl = mur_jctl->next_gen;
					rctl->jctl = mur_jctl;
					mur_jctl->rec_offset = mur_jctl->lvrec_off; /* Start from last record */
				} else
				{
					mur_jctl = rctl->jctl = rctl->jctl_save_turn_around;
					mur_jctl->rec_offset = mur_jctl->save_turn_around_offset;
				}
			} else	/* recover interrupted earlier */
			{	/* We already called mur_apply_pblk() to undo recover generated PBLKs.
			 	 * Later we followed the next_jnl_file_name links to setup jctl list for this region.
				 * We later called mur_back_process() to resolve transactions using the new turn-around point,
				 * but mur_back_process() did not apply PBLKs for interrupted recovery (even for NOVERIFY).
				 * Last time we called this routine, we set rctl->jctl_save_turn_around.
				 * Now we are in the phase to apply original GT.M generated PBLKs.
				 * We skip application of PBLKs till the last recover's turn-around point.
				 */
				mur_jctl = rctl->jctl = rctl->jctl_save_turn_around;
				assert(mur_jctl->save_turn_around_offset);
				mur_jctl->rec_offset = mur_jctl->save_turn_around_offset;
				DEBUG_ONLY(
					/* assert that first pass turn-around-point is later than the final turn-around-point */
					for (tmpjctl = mur_jctl; NULL != tmpjctl && tmpjctl != rctl->jctl_turn_around;
						tmpjctl = tmpjctl->prev_gen)
						;
					assert(NULL != tmpjctl && ((tmpjctl != mur_jctl) ||
						(mur_jctl->rec_offset >= mur_jctl->turn_around_offset)));
				)
			}
			if (mur_options.verify || rctl->jfh_recov_interrupted)
			{	/* if going to apply pblks then store prospective turnaround point now itself
				 * so we remember to undo PBLKs at least upto here in case this recovery is interrupted.
				 * in case of normal recovery with -noverify, we would have written this information
				 * out in mur_back_process() itself so we do not need to write it again here.
				 */
				rctl->csd->intrpt_recov_tp_resolve_time = murgbl.tp_resolve_time;
				rctl->csd->intrpt_recov_resync_seqno = (murgbl.resync_seqno ? murgbl.resync_seqno : 0);
				/* flush the changed csd to disk */
				fc = rctl->gd->dyn.addr->file_cntl;
				fc->op = FC_WRITE;
				fc->op_buff = (sm_uc_ptr_t)rctl->csd;
				fc->op_len = ROUND_UP(sizeof(sgmnt_data), DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
			}
		} else
		{	/* Recover was interrupted earlier. We are in the phase to apply interrupted recovery generated PBLKs */
			assert(NULL == rctl->jctl_turn_around);
			if (!rctl->jfh_recov_interrupted)
				continue;
			mur_jctl = rctl->jctl;	/* Latest generation */
			assert(NULL == mur_jctl->next_gen);
			mur_jctl->rec_offset = mur_jctl->lvrec_off; /* Start from last record */
		}
		assert(0 != mur_jctl->rec_offset);
		if (mur_options.verbose)
			gtm_putmsg(VARLSTCNT(15) ERR_MUJNINFO, 13, LEN_AND_LIT("Mur_apply_pblk:start  "),
			mur_jctl->jnl_fn_len, mur_jctl->jnl_fn, mur_jctl->rec_offset,
			murgbl.tp_resolve_time, mur_jctl->turn_around_offset, mur_jctl->turn_around_time,
			mur_jctl->turn_around_tn, &mur_jctl->turn_around_seqno,
			0, murgbl.token_table.count, murgbl.broken_cnt);
		for (status = mur_prev(mur_jctl->rec_offset); SS_NORMAL == status; status = mur_prev_rec())
		{
			if (apply_intrpt_pblk && !mur_jctl->jfh->recover_interrupted && NULL == rctl->jctl_alt_head)
			{
				assert(NULL != mur_jctl->next_gen);
				assert(mur_jctl->next_gen->jfh->recover_interrupted);
				rctl->jctl_alt_head = mur_jctl->next_gen;
				mur_jctl->next_gen = NULL;
			}
			if (JRT_EPOCH == mur_rab.jnlrec->prefix.jrec_type)
			{
				if (!apply_intrpt_pblk)
				{
					if ((mur_jctl == rctl->jctl_turn_around)
							&& (mur_jctl->rec_offset <= mur_jctl->turn_around_offset))
					{	/* jctl->rec_offset can be different from jctl->turn_around_offset in case of
						 * mur_ztp_lookback() processing. but we are guaranteed an epoch at the start
						 * of every journal file, so we should encounter an epoch in the same journal
						 * file as rctl->jctl_turn_around. we have now reached the turn-around point.
						 * Note that the following assignments should parallel those done in
						 * mur_back_process on reaching the turn-around point.
						 */
						assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
							|| (mur_jctl->turn_around_time == mur_rab.jnlrec->prefix.time));
						assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
						    || (mur_jctl->turn_around_seqno == mur_rab.jnlrec->jrec_epoch.jnl_seqno));
						assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
							|| (mur_jctl->turn_around_tn == ((jrec_prefix *)mur_rab.jnlrec)->tn));
						rctl->jctl_turn_around = mur_jctl;
						mur_jctl->turn_around_offset = mur_jctl->rec_offset;
						mur_jctl->turn_around_time = mur_rab.jnlrec->prefix.time;
						mur_jctl->turn_around_seqno = mur_rab.jnlrec->jrec_epoch.jnl_seqno;
						mur_jctl->turn_around_tn = ((jrec_prefix *)mur_rab.jnlrec)->tn;
						break;
					}
				} else
				{
					if (mur_jctl->rec_offset == mur_jctl->jfh->turn_around_offset)
					{	/* we reached the turn-around point of last interrupted recovery */
						assert(mur_jctl->jfh->turn_around_time == mur_rab.jnlrec->prefix.time);
						assert(rctl->jctl_head == mur_jctl);
						rctl->jctl_save_turn_around = mur_jctl;
						mur_jctl->save_turn_around_offset = mur_jctl->rec_offset;
						break;
					} else if (mur_jctl->rec_offset < mur_jctl->jfh->turn_around_offset)
					{
						gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len,
								mur_jctl->jnl_fn, mur_jctl->rec_offset);
						return ERR_JNLBADRECFMT;
					}
				}
			} else if (JRT_PBLK == mur_rab.jnlrec->prefix.jrec_type && SS_NORMAL != (status = mur_output_pblk()))
			{	/* if in interrupted pblk applying phase (apply_intrpt_pblk is TRUE), it is possible that we
				 * would be playing PBLKs of recover-created as well as GT.M created journal files. this is
				 * necessary until we reach the saved turn-around point of the previous interrupted recovery.
				 * example of why we need to play GT.M generated (in addition to recover generated PBLKs) is below.
				 *
				 * Assume GT.M crashed and
				 * 	journal file layout now is a_1.mjl <-- a.mjl.
				 * First recovery found turn-around point in a.mjl so it renamed a.mjl to a_2.mjl and created
				 * 	a.mjl and played a few post-turn-around-point records into a.mjl when it was interrupted
				 * 	journal file layout now is a_1.mjl <-- a_2.mjl <-- a.mjl
				 * Second recovery had a specified turn-around point which was in a_1.mjl and it took the
				 * 	minimum of the specified and saved (in a_2.mjl) turn-around points and undid PBLKs
				 * 	upto a_1.mjl and was about to create a new a.mjl (which pointed back to a_1.mjl) after
				 * 	renaming the current a.mjl, but crashed before the rename. Note that at this point a_1.mjl
				 * 	has a non-zero turn-around-offset set and the database has been rolled back to a_1.mjl.
				 * 	journal file layout now is a_1.mjl <-- a_2.mjl <-- a.mjl
				 * Third recovery is now attempted. This will do interrupted PBLK processing (now upto the
				 * 	saved turn-around-offset which is in a_1.mjl). It has to undo PBLKs of a.mjl, a_2.mjl and
				 * 	a_1.mjl in the process of reaching there. If instead it undid only PBLKs of recover-created
				 * 	journal files (which will be only a.mjl) and went to the saved turn-around-offset in
				 * 	a_1.mjl, we would have rolled back the database to a state as of the end of a_2.mjl
				 * 	although a previous recovery had rolled the database back to a previous generation (a_1.mjl)
				 * This will mean we left out playing PBLKs in a_2.mjl and a_1.mjl which can cause integrity errors.
				 */
				return status;
			}
		}
		if (SS_NORMAL != status)
		{
			if (ERR_NOPREVLINK == status)
				gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
			else if (ERR_JNLREADBOF == status)
				gtm_putmsg(VARLSTCNT(4) ERR_JNLREADBOF, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
			return status;
		}
		if (mur_options.verbose)
			gtm_putmsg(VARLSTCNT(15) ERR_MUJNINFO, 13,
			LEN_AND_LIT("Mur_apply_pblk:intrpt:trnarnd "), mur_jctl->jnl_fn_len, mur_jctl->jnl_fn,
			mur_jctl->rec_offset, murgbl.tp_resolve_time, mur_jctl->turn_around_offset, mur_jctl->turn_around_time,
			mur_jctl->turn_around_tn, &mur_jctl->turn_around_seqno, 0, murgbl.token_table.count, murgbl.broken_cnt);
		UNIX_ONLY(
		gv_cur_region = rctl->csa->region;
		udi = FILE_INFO(gv_cur_region);
		DB_FSYNC(gv_cur_region, udi, rctl->csa, db_fsync_in_prog);
		)
	}
	return SS_NORMAL;
}

uint4 mur_output_pblk(void)
{
	file_control	*db_ctl;

	db_ctl =  mur_ctl[mur_regno].db_ctl;
	/* apply PBLKs to database of mur_ctl[mur_regno].
	 * This only takes place during rollback/recover, and is thus the first restoration being done to the database;
	 * therefore, it will not cause a conflict with the write cache, as the cache will be empty
	 */
	db_ctl->op = FC_WRITE;
	db_ctl->op_buff = (uchar_ptr_t)mur_rab.jnlrec->jrec_pblk.blk_contents;
	db_ctl->op_len = mur_rab.jnlrec->jrec_pblk.bsiz;
	db_ctl->op_pos = mur_ctl[mur_regno].csd->blk_size / DISK_BLOCK_SIZE * mur_rab.jnlrec->jrec_pblk.blknum
					  + mur_ctl[mur_regno].csd->start_vbn;
	return (dbfilop(db_ctl));
}
