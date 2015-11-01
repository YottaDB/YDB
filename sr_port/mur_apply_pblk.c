/****************************************************************
 *								*
 *	Copyright 2003, 2005 Fidelity Information Services, Inc	*
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
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "dbfilop.h"
#include "gds_blk_downgrade.h"

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
	inctn_opcode_t		opcode;
	struct_jrec_inctn	*inctn_rec;
	enum jnl_record_type 	rectype;

        UNIX_ONLY(unix_db_info   *udi;)

	error_def(ERR_JNLREAD);
	error_def(ERR_JNLREADBOF);
	error_def(ERR_JNLBADRECFMT);
	error_def(ERR_NOPREVLINK);
	error_def(ERR_MUINFOUINT4);
	error_def(ERR_MUINFOUINT8);
	error_def(ERR_MUINFOSTR);

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
					mur_jctl = rctl->jctl = rctl->jctl_apply_pblk;
					assert(NULL != mur_jctl);
					mur_jctl->rec_offset = mur_jctl->apply_pblk_stop_offset;
				}
			} else	/* recover interrupted earlier */
			{	/* We already called mur_apply_pblk() to undo recover generated PBLKs.
			 	 * Later we followed the next_jnl_file_name links to setup jctl list for this region.
				 * We later called mur_back_process() to resolve transactions using the new turn-around point,
				 * but mur_back_process() did not apply PBLKs for interrupted recovery (even for NOVERIFY).
				 * Last time we called this routine, we set rctl->jctl_apply_pblk.
				 * Now we are in the phase to apply original GT.M generated PBLKs.
				 * We skip application of PBLKs till the last recover's turn-around point.
				 */
				mur_jctl = rctl->jctl = rctl->jctl_apply_pblk;
				assert(mur_jctl->apply_pblk_stop_offset);
				mur_jctl->rec_offset = mur_jctl->apply_pblk_stop_offset;
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
				fc->op_len = ROUND_UP(SGMNT_HDR_LEN, DISK_BLOCK_SIZE);
				fc->op_pos = 1;
				dbfilop(fc);
			}
		} else
		{
			assert(NULL == rctl->jctl_turn_around);
			if (!rctl->jfh_recov_interrupted)
				continue;
			/* Recover was interrupted earlier. We are in the phase to apply interrupted recovery generated PBLKs.
			 * In interrupted pblk applying phase, it is possible that we would be playing PBLKs of recover-created
			 * as well as GT.M created journal files. this is necessary until we reach the saved turn-around point
			 * of the previous interrupted recovery.
			 *
			 * Example of why we need to play GT.M generated (in addition to recover generated PBLKs) is below.
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
			mur_jctl = rctl->jctl;	/* Latest generation */
			assert(NULL == mur_jctl->next_gen);
			mur_jctl->rec_offset = mur_jctl->lvrec_off; /* Start from last record */
		}
		for ( ; ;)
		{
			assert(0 != mur_jctl->rec_offset);
			if (!apply_intrpt_pblk)
			{
				PRINT_VERBOSE_STAT("mur_apply_blk:start");
			} else
			{
				PRINT_VERBOSE_STAT("mur_apply_blk:start : Apply Interrupted PBLK");
			}
			for (status = mur_prev(mur_jctl->rec_offset), mur_jctl->after_end_of_data = TRUE;
								SS_NORMAL == status; status = mur_prev_rec())
			{
				rectype = mur_rab.jnlrec->prefix.jrec_type;
				mur_jctl->after_end_of_data = mur_jctl->after_end_of_data &&
					(mur_jctl->rec_offset >= mur_jctl->jfh->end_of_data);
				if (apply_intrpt_pblk)
				{
					if (NULL == rctl->jctl_alt_head && !mur_jctl->jfh->recover_interrupted)
					{
						assert(NULL != mur_jctl->next_gen);
						assert(mur_jctl->next_gen->jfh->recover_interrupted);
						rctl->jctl_alt_head = mur_jctl->next_gen;/* Save the recover generated journal
												files we finished processing */
						mur_jctl->next_gen = NULL; /* Since we do not want to process them again */
					}
					if (JRT_INCTN == rectype)
						MUR_INCTN_BLKS_TO_UPGRD_ADJUST(rctl, mur_rab);
				}
				if (JRT_EPOCH == rectype)
				{
					assert(NULL != rctl->csd);
					if (!apply_intrpt_pblk)
					{
						if ((mur_jctl == rctl->jctl_turn_around)
								&& (mur_jctl->rec_offset <= mur_jctl->turn_around_offset))
						{	/* jctl->rec_offset can be different from jctl->turn_around_offset in
							 * case of mur_ztp_lookback() processing. But we are guaranteed an epoch
							 * at the start of every journal file, so we should encounter an epoch
							 * in the same journal file as rctl->jctl_turn_around. We have now reached
							 * the turn-around point.
							 * Note that the following assignments should parallel those done in
							 * mur_back_process on reaching the turn-around point.
							 */
							assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
								|| (mur_jctl->turn_around_time == mur_rab.jnlrec->prefix.time));
							assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
							    || (mur_jctl->turn_around_seqno ==
							    	mur_rab.jnlrec->jrec_epoch.jnl_seqno));
							assert((mur_jctl->rec_offset != mur_jctl->turn_around_offset)
								|| (mur_jctl->turn_around_tn ==
									((jrec_prefix *)mur_rab.jnlrec)->tn));
							rctl->jctl_turn_around = mur_jctl;
							mur_jctl->turn_around_offset = mur_jctl->rec_offset;
							mur_jctl->turn_around_time = mur_rab.jnlrec->prefix.time;
							mur_jctl->turn_around_seqno = mur_rab.jnlrec->jrec_epoch.jnl_seqno;
							mur_jctl->turn_around_tn = ((jrec_prefix *)mur_rab.jnlrec)->tn;
							/* Reset file-header "blks_to_upgrd" counter to the turn around point
							 * epoch value.  Later in mur_process_intrpt_recov, this will be updated
							 * to include the number of new V4 format bitmaps created by
							 * post-turnaround-point db file extensions.
							 * This will also be flushed to disk in that routine.
							 * The adjustment value is maintained in rctl->blks_to_upgrd_adjust.
							 */
							rctl->csd->blks_to_upgrd = mur_rab.jnlrec->jrec_epoch.blks_to_upgrd;
							break;
						}
					} else
					{
						if (mur_jctl->rec_offset == mur_jctl->jfh->turn_around_offset)
						{	/* we reached the turn-around point of last interrupted recovery */
							assert(mur_jctl->jfh->turn_around_time == mur_rab.jnlrec->prefix.time);
							assert(rctl->jctl_head == mur_jctl);
							/* note down the fact that we have applied PBLKs upto this point */
							rctl->jctl_apply_pblk = mur_jctl;
							mur_jctl->apply_pblk_stop_offset = mur_jctl->rec_offset;
							break;
						} else if (mur_jctl->rec_offset < mur_jctl->jfh->turn_around_offset)
						{
							PRINT_VERBOSE_STAT("mur_apply_blk:turn_around_offset is bad");
							gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len,
									mur_jctl->jnl_fn, mur_jctl->rec_offset);
							return ERR_JNLBADRECFMT;
						}
					}
				} else if ((JRT_PBLK == rectype) && (SS_NORMAL != (status = mur_output_pblk())))
				{
					PRINT_VERBOSE_STAT("mur_apply_blk:mur_output_pblk failed");
					return status;
				}
			}
			PRINT_VERBOSE_STAT("mur_apply_blk:end");
			if (SS_NORMAL == status)
				break;
			if (ERR_NOPREVLINK == status)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_NOPREVLINK, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
				return ERR_NOPREVLINK;
			} else if (ERR_JNLREADBOF == status)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_JNLREADBOF, 2, mur_jctl->jnl_fn_len, mur_jctl->jnl_fn);
				return ERR_JNLREADBOF;
			} else if (ERR_JNLREAD == status) /* This message is already issued in mur_read_file */
				return ERR_JNLREAD;
			if ((NULL != mur_jctl->next_gen) || (mur_jctl->rec_offset < mur_jctl->jfh->end_of_data))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_JNLBADRECFMT, 3, mur_jctl->jnl_fn_len,
								mur_jctl->jnl_fn, mur_jctl->rec_offset);
				return status;
			}
			/* We are in the interrupted pblk application phase and applying either interrupted recovery
			 * generated pblks or GT.M generated pblks and encounter bad records in the tail of the
			 * last generation journal file that was active during the crash. Skip those and continue. */
			PRINT_VERBOSE_TAIL_BAD(mur_options, mur_jctl);
			if (SS_NORMAL != mur_fread_eof_crash(mur_jctl, mur_jctl->jfh->end_of_data, mur_jctl->rec_offset))
				return ERR_JNLBADRECFMT;
		} /* end infinite for */
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
	reg_ctl_list	*rctl;
	file_control	*db_ctl;
	struct_jrec_blk	pblkrec;
	uchar_ptr_t	pblkcontents;

	rctl = &mur_ctl[mur_regno];
	pblkrec = mur_rab.jnlrec->jrec_pblk;
	/* note that all fields in the "jrec_pblk" typedef structure are now referencible from the local variable "pblkrec".
	 * the only exception is "blk_contents" which is a hung buffer at the end of the structure.
	 * copy that address in a local variable "pblkcontents" separately.
	 */
	pblkcontents = (uchar_ptr_t)&mur_rab.jnlrec->jrec_pblk.blk_contents[0];
	if (IS_GDS_BLK_DOWNGRADE_NEEDED(pblkrec.ondsk_blkver))
	{	/* This block was not in GDSVCURR format before the GT.M update wrote this PBLK record. But since all buffers in
		 * the cache are stored in GDSVCURR format, the before-image in the PBLK record is in GDSVCURR
		 * format. In order to really undo the update, downgrade the before-image before playing it back.
		 * This can thankfully be done inline (i.e. using the same buffer) due to the following reasons.
		 *	a) The reformat routine allows for the source and target buffers to be the same AND
		 *	b) The block downgrade routine always needs less space for the target buffer than the source buffer AND
		 *	c) Recovery does not rely on the blk_contents of a PBLK journal record other than in this routine.
		 */
		 gds_blk_downgrade((v15_blk_hdr_ptr_t)pblkcontents, (blk_hdr_ptr_t)pblkcontents);
	}
	db_ctl =  rctl->db_ctl;
	/* apply PBLKs to database of mur_ctl[mur_regno].
	 * This only takes place during rollback/recover, and is thus the first restoration being done to the database;
	 * therefore, it will not cause a conflict with the write cache, as the cache will be empty
	 */
	db_ctl->op = FC_WRITE;
	db_ctl->op_buff = (uchar_ptr_t)pblkcontents;
	/* Use jrec size even if downgrade may have shrunk block. If the block has an integ error, we don't run into
	 * any trouble.
	*/
	db_ctl->op_len = pblkrec.bsiz;
	db_ctl->op_pos = mur_ctl[mur_regno].csd->blk_size / DISK_BLOCK_SIZE * pblkrec.blknum + mur_ctl[mur_regno].csd->start_vbn;
	return (dbfilop(db_ctl));
}
