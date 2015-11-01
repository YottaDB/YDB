/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*******************************************************************************
*
*       MODULE NAME:            DSE_DMP_FHEAD
*
*       CALLING SEQUENCE:       void dse_dmp_fhead ()
*
*       DESCRIPTION:    This module dumps certain fields of current file
*                       header.
*
*       HISTORY:
*
*******************************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include <math.h> /* needed for handling of epoch_interval (EPOCH_SECOND2SECOND macro uses ceil) */

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "cli.h"
#include "util.h"
#include "dse.h"
#include "dse_puttime.h"
#include "gtmmsg.h"
#include "stringpool.h"		/* for GET_CURR_TIME_IN_DOLLARH_AND_ZDATE macro */
#include "op.h"

#define MAX_UTIL_LEN    	64
#define NEXT_EPOCH_TIME_SPACES	"                   " /* 19 spaces, we have 19 character field width to output Next Epoch Time */

GBLREF sgmnt_addrs      *cs_addrs;
GBLREF gd_region        *gv_cur_region;

GBLDEF mval dse_dmp_time_fmt = DEFINE_MVAL_LITERAL(MV_STR, 0, 0, STR_LIT_LEN(DSE_DMP_TIME_FMT), DSE_DMP_TIME_FMT, 0, 0);

LITREF	char		*jrt_label[JRT_RECTYPES];

#define SHOW_STAT(TEXT, VARIABLE)       if (0 != csd->VARIABLE.evnt_cnt) 					\
	util_out_print(TEXT"  0x!8XL        Transaction =           0x!8XL", TRUE, (csd->VARIABLE.evnt_cnt),	\
		(csd->VARIABLE.evnt_tn));

#define SHOW_DB_CSH_STAT(COUNTER, TEXT1, TEXT2)									\
	if (csd->COUNTER.curr_count || csd->COUNTER.cumul_count)				\
	{													\
		util_out_print(TEXT1"  0x!8XL      "TEXT2"  0x!8XL", TRUE, (csd->COUNTER.curr_count), \
				(csd->COUNTER.cumul_count + csd->COUNTER.curr_count));	\
	}

/* NEED_TO_DUMP is only for the qualifiers other than "BASIC" and "ALL".
	file_header is not dumped only if "NOBASIC" is explicitly specified */

#define	NEED_TO_DUMP(string)				\
	(CLI_PRESENT == cli_present(string) || CLI_PRESENT == cli_present("ALL") && CLI_NEGATED != cli_present(string))

void dse_dmp_fhead (void)
{
	boolean_t		jnl_buff_open;
	unsigned char		util_buff[MAX_UTIL_LEN], buffer[MAXNUMLEN], *ptr;
	int			util_len, rectype, time_len;
	uint4			jnl_status;
	enum jnl_state_codes	jnl_state;
	gds_file_id		zero_fid;
	mval			dollarh_mval, zdate_mval;
	char			dollarh_buffer[MAXNUMLEN], zdate_buffer[sizeof(DSE_DMP_TIME_FMT)];
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	backup_buff_ptr_t	bptr;

	csa = cs_addrs;
	csd = csa->hdr;
        jnl_state = (uint4)csd->jnl_state;
	VMS_ONLY(
		memset(&zero_fid, 0, sizeof(zero_fid));
		jnl_buff_open = (0 != memcmp(csa->nl->jnl_file.jnl_file_id.fid, zero_fid.fid, sizeof(zero_fid.fid)));
	)
	UNIX_ONLY(
		jnl_buff_open = (0 != csa->nl->jnl_file.u.inode);
	)
	if (CLI_NEGATED != cli_present("BASIC"))
	{
		util_out_print("!/File            !AD", TRUE, gv_cur_region->dyn.addr->fname_len,
			&gv_cur_region->dyn.addr->fname[0]);
		util_out_print("Region          !AD", TRUE, gv_cur_region->rname_len, &gv_cur_region->rname[0]);
		GET_CURR_TIME_IN_DOLLARH_AND_ZDATE(dollarh_mval, dollarh_buffer, zdate_mval, zdate_buffer);
		util_out_print("Date/Time       !AD [$H = !AD]", TRUE, zdate_mval.str.len, zdate_mval.str.addr,
				dollarh_mval.str.len, dollarh_mval.str.addr);
		util_out_print("  Access method                   !AD", FALSE, 2, (csd->acc_meth == dba_mm) ? "MM" : "BG");
		util_out_print("      ", FALSE);
		util_out_print("  Global Buffers        !12UL", TRUE, csd->n_bts);
		util_out_print("  Reserved Bytes        !12UL", FALSE, csd->reserved_bytes);
		util_out_print("      ", FALSE);
		util_out_print("  Block size (in bytes) !12UL", TRUE, csd->blk_size);
		util_out_print("  Maximum record size   !12UL", FALSE, csd->max_rec_size);
		util_out_print("      ", FALSE);
		util_out_print("  Starting VBN          !12UL", TRUE, csd->start_vbn);
		util_out_print("  Maximum key size      !12UL", FALSE, csd->max_key_size);
		util_out_print("      ", FALSE);
		util_out_print("  Total blocks            0x!8XL", TRUE, csa->ti->total_blks);
		util_out_print("  Null subscripts       !AD", FALSE, 12, (csd->null_subs) ? "        TRUE" : "       FALSE");
		util_out_print("      ", FALSE);
		util_out_print("  Free blocks             0x!8XL", TRUE, csa->ti->free_blocks);
		util_out_print("  Last Record Backup      0x!8XL", FALSE, csd->last_rec_backup);
		util_out_print("      ", FALSE);
		util_out_print ("  Extension Count       !12UL", TRUE, csd->extension_size);
		util_out_print("  Last Database Bckup     0x!8XL", FALSE, csd->last_com_backup);
		util_out_print("      ", FALSE);
		if (csd->bplmap > 0)
			util_out_print("  Number of local maps  !12UL", TRUE,
				(csa->ti->total_blks + csd->bplmap - 1) / csd->bplmap);
		else
			util_out_print("  Number of local maps            ??", TRUE);
		util_out_print("  Last Bytestream Bckup   0x!8XL", FALSE, csd->last_inc_backup);
		util_out_print("      ", FALSE);
		util_out_print("  Lock space              0x!8XL", TRUE, csd->lock_space_size/OS_PAGELET_SIZE);
		util_out_print("  In critical section     0x!8XL", FALSE, csa->nl->in_crit);
		util_out_print("      ", FALSE);
		util_out_print("  Timers pending        !12UL", TRUE, csa->nl->wcs_timers + 1);
		if (FROZEN_BY_ROOT == csd->freeze)
			util_out_print("  Cache freeze id     FROZEN BY ROOT", FALSE);
		else
			util_out_print("  Cache freeze id         0x!8XL", FALSE, (csd->freeze)? csd->freeze : 0);
		util_out_print("      ", FALSE);
		dse_puttime(csd->flush_time, "  Flush timer            !AD", TRUE);
		util_out_print("  Freeze match            0x!8XL", FALSE, (csd->image_count)? csd->image_count : 0);
		util_out_print("      ", FALSE);
		util_out_print("  Flush trigger         !12UL", TRUE, csd->flush_trigger);
		util_out_print("  Current transaction     0x!8XL", FALSE, csa->ti->curr_tn);
		util_out_print("      ", FALSE);
		util_out_print("  No. of writes/flush   !12UL", TRUE, csd->n_wrt_per_flu);
		if (csd->def_coll)
		{
			util_out_print("  Default Collation     !12UL", FALSE, csd->def_coll);
			util_out_print("      ", FALSE);
			util_out_print("  Collation Version     !12UL", TRUE, csd->def_coll_ver);
		}
		util_out_print("  Create in progress    !AD", FALSE, 12, (csd->createinprogress) ? "        TRUE" : "       FALSE");
		util_out_print("      ", FALSE);

#  ifdef CNTR_WORD_32
		util_out_print("  Modified cache blocks !12UL", TRUE, csa->nl->wcs_active_lvl);
#  else
		util_out_print("  Modified cache blocks !12UW", TRUE, csa->nl->wcs_active_lvl);
#  endif

		util_out_print("  Reference count       !12UL", FALSE, csa->nl->ref_cnt);
		util_out_print("      ", FALSE);
		util_out_print("  Wait Disk             !12UL", TRUE, csd->wait_disk_space);
		util_out_print("  Journal State        !AD", (jnl_notallowed == jnl_state), 13,
				(jnl_notallowed != jnl_state) ?
				((jnl_state == jnl_closed) ? "          OFF"
				 : (jnl_buff_open ? "           ON" : "[inactive] ON")) : "     DISABLED");
		if (jnl_notallowed != jnl_state)
		{
			util_out_print("      ", FALSE);
			util_out_print("  Journal Before imaging       !AD", TRUE, 5, (csd->jnl_before_image) ? " TRUE" : "FALSE");
			util_out_print("  Journal Allocation    !12UL", FALSE, csd->jnl_alq);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Extension     !12UL", TRUE, csd->jnl_deq);
			util_out_print("  Journal Buffer Size   !12UL", FALSE, csd->jnl_buffer_size);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Alignsize     !12UL", TRUE, csd->alignsize / DISK_BLOCK_SIZE);
			util_out_print("  Journal AutoSwitchLimit !10UL", FALSE, csd->autoswitchlimit);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Epoch Interval!12UL", TRUE, EPOCH_SECOND2SECOND(csd->epoch_interval));
#ifdef UNIX
			util_out_print("  Journal Yield Limit   !12UL", FALSE, csd->yield_lmt);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Sync IO              !AD", TRUE, 5, (csd->jnl_sync_io ? " TRUE" : "FALSE"));
#endif
			util_out_print("  Journal File: !AD", TRUE, JNL_LEN_STR(csd));
		}
		if (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb)
			util_out_print("  Online Backup NBB     !12UL", TRUE, csa->nl->nbb);
		/* Mutex Stuff */
		util_out_print("  Mutex Hard Spin Count !12UL", FALSE, csd->mutex_spin_parms.mutex_hard_spin_count);
		util_out_print("      ", FALSE);
		util_out_print("  Mutex Sleep Spin Count!12UL", TRUE, csd->mutex_spin_parms.mutex_sleep_spin_count);
		util_out_print("  Mutex Spin Sleep Time !12UL", FALSE,
			(csd->mutex_spin_parms.mutex_spin_sleep_mask == 0) ?
				0 : (csd->mutex_spin_parms.mutex_spin_sleep_mask + 1));
		util_out_print("      ", FALSE);
		util_out_print("  KILLs in progress     !12UL", TRUE, csd->kill_in_prog);
		util_out_print("  Replication State           !AD", FALSE, 6,
			(csd->repl_state == repl_closed)? "   OFF" : "    ON");
		ptr = i2asclx(buffer, csd->reg_seqno);
#ifndef __vax
		util_out_print("        Region Seqno    0x!16@XJ", TRUE, &csd->reg_seqno);
		util_out_print("  Resync Seqno    0x!16@XJ", FALSE, &csd->resync_seqno);
#else
		util_out_print("        Region Seqno    0x00000000!8@XJ", TRUE, &csd->reg_seqno);
		util_out_print("  Resync Seqno    0x00000000!8@XJ", FALSE, &csd->resync_seqno);
#endif
		util_out_print("        Resync transaction      0x!8XL", TRUE, csd->resync_tn);
	}
	if (NEED_TO_DUMP("ENVIRONMENT"))
	{
                util_out_print(0, TRUE);
		util_out_print("  Full Block Writes           !AD", FALSE, 6,
			(csa->do_fullblockwrites) ? "    ON" : "   OFF");
		util_out_print("      ", FALSE);
		util_out_print("  Full Block Write Len  !12UL", TRUE, csa->fullblockwrite_len);
	}
	if (NEED_TO_DUMP("DB_CSH"))
	{
                util_out_print(0, TRUE);
#define TAB_DB_CSH_ACCT_REC(COUNTER,TEXT1,TEXT2)	SHOW_DB_CSH_STAT(COUNTER, TEXT1, TEXT2)
#include "tab_db_csh_acct_rec.h"
#undef TAB_DB_CSH_ACCT_REC
	}
	if (NEED_TO_DUMP("RETRIES"))
	{
                util_out_print(0, TRUE);
                util_out_print("  Retries [0]           !12UL", TRUE, csd->n_retries[0]);
                util_out_print("  Retries [1]           !12UL", TRUE, csd->n_retries[1]);
                util_out_print("  Retries [2]           !12UL", TRUE, csd->n_retries[2]);
                util_out_print("  Retries [3]           !12UL", TRUE, csd->n_retries[3]);
	}
	if (NEED_TO_DUMP("TPRETRIES"))
	{
#define	DUMP_TP_RETRIES(X)										\
	util_out_print("  Total TP Retries ["#X"]  !12UL        Cnflct TP Retries ["#X"] !12UL",	\
			TRUE, csd->n_tp_retries[X], csd->n_tp_retries_conflicts[X]);
                util_out_print(0, TRUE);
		DUMP_TP_RETRIES(0);
		DUMP_TP_RETRIES(1);
		DUMP_TP_RETRIES(2);
		DUMP_TP_RETRIES(3);
		DUMP_TP_RETRIES(4);
		DUMP_TP_RETRIES(5);
		DUMP_TP_RETRIES(6);
#undef DUMP_TP_RETRIES
	}
	if (NEED_TO_DUMP("GVSTATS"))
	{
                util_out_print(0, TRUE);
		util_out_print("  Number of        : GET                      : 0x!8XL", TRUE, csd->n_gets);
		util_out_print("  Number of        : ORDER                    : 0x!8XL", TRUE, csd->n_order);
		util_out_print("  Number of        : QUERY                    : 0x!8XL", TRUE, csd->n_queries);
		util_out_print("  Number of        : ZPREV                    : 0x!8XL", TRUE, csd->n_zprevs);
		util_out_print("  Number of        : DATA                     : 0x!8XL", TRUE, csd->n_data);
		util_out_print("  Number of        : KILL                     : 0x!8XL", TRUE, csd->n_kills);
		util_out_print("  Number of Non-TP : SET                      : 0x!8XL", TRUE, csd->n_puts);
		util_out_print("  Number of Non-TP : SET (duplicate SETS)     : 0x!8XL", TRUE, csd->n_puts_duplicate);
		util_out_print("  Number of     TP : UPDATES                  : 0x!8XL", TRUE, csd->n_tp_updates);
		util_out_print("  Number of     TP : UPDATES (duplicate SETS) : 0x!8XL", TRUE, csd->n_tp_updates_duplicate);
	}
	if (NEED_TO_DUMP("TPBLKMOD"))
	{
                util_out_print(0, TRUE);
		assert(n_tp_blkmod_types < ARRAYSIZE(csd->tp_cdb_sc_blkmod));
                util_out_print("  TP blkmod gvcst_put   !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_gvcst_put]);
                util_out_print("  TP blkmod gvcst_srch  !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_gvcst_srch]);
                util_out_print("  TP blkmod t_qread     !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_t_qread]);
                util_out_print("  TP blkmod tp_tend     !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_tp_tend]);
                util_out_print("  TP blkmod tp_hist     !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_tp_hist]);
	}
	if (NEED_TO_DUMP("BG_TRC"))
	{
                util_out_print(0, TRUE);
		/* print out all the BG_TRACE accounting fields */
#define TAB_BG_TRC_REC(A,B)	SHOW_STAT(A,B);
#include "tab_bg_trc_rec_fixed.h"
#include "tab_bg_trc_rec_variable.h"
#undef TAB_BG_TRC_REC
	}
	jpc = csa->jnl;
	if (NEED_TO_DUMP("JOURNAL") && (JNL_ENABLED(csd) && (NULL != jpc) && (NULL != jpc->jnl_buff)))
	{
		jb = jpc->jnl_buff;
		if (jnl_buff_open)
		{
			util_out_print(0, TRUE);
			/* --------------------------- journal buffer --------------------------------- */
			util_out_print("  Jnl Buffer Size       !12UL", FALSE, jb->size);
			util_out_print("      ", FALSE);
			util_out_print("  Dskaddr               !12UL", TRUE, jb->dskaddr);
			util_out_print("  Free                  !12UL", FALSE, jb->free);
			util_out_print("      ", FALSE);
			util_out_print("  Freeaddr              !12UL", TRUE, jb->freeaddr);
			util_out_print("  Dsk                   !12UL", FALSE, jb->dsk);
			util_out_print("      ", FALSE);
			util_out_print("  Wrtsize               !12UL", TRUE, jb->wrtsize);
			util_out_print("                                    ", FALSE);
			util_out_print("      ", FALSE);
			util_out_print("  Min_write_size        !12UL", TRUE, jb->min_write_size);
			util_out_print("  bytcnt                !12UL", FALSE, jb->bytcnt);
			util_out_print("      ", FALSE);
			util_out_print("  Max_write_size        !12UL", TRUE, jb->max_write_size);
			util_out_print("  Before image                 !AD", FALSE, 5, (jb->before_images ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  Filesize              !12UL", TRUE, jb->filesize);
			util_out_print("  Iosb.cond             !12UW", FALSE, jb->iosb.cond);
			util_out_print("      ", FALSE);
			util_out_print("  qiocnt                !12UL", TRUE, jb->qiocnt);
			util_out_print("  Iosb.length           !12UW", FALSE, jb->iosb.length);
			util_out_print("      ", FALSE);
			util_out_print("  errcnt                !12UL", TRUE, jb->errcnt);
			util_out_print("  Iosb.dev_specific     !12UL", FALSE, jb->iosb.dev_specific);
			util_out_print("      ", FALSE);
			time_len = exttime(jb->next_epoch_time, (char *)buffer, 0);
			assert(STR_LIT_LEN(NEXT_EPOCH_TIME_SPACES) >= time_len);
			util_out_print("  Next Epoch_Time!AD!AD", TRUE, STR_LIT_LEN(NEXT_EPOCH_TIME_SPACES) - time_len + 1,
					NEXT_EPOCH_TIME_SPACES, time_len - 1, buffer); /* -1 to avoid printing \ at end of $H
											* format time returned by exttime */
			util_out_print("  Blocked Process       !12UL", FALSE, jb->blocked);
			util_out_print("      ", FALSE);
			util_out_print("  Epoch_tn                0x!8XL", TRUE, jb->epoch_tn);
			util_out_print("  Io_in_progress               !AD", FALSE, 5,
			  (jb->UNIX_ONLY(io_in_prog_latch.latch_pid)VMS_ONLY(io_in_prog) ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  Epoch_Interval        !12UL", TRUE, EPOCH_SECOND2SECOND(jb->epoch_interval));
			util_out_print("  Now_writer            !12UL", FALSE,
				       (jb->UNIX_ONLY(io_in_prog_latch.latch_pid)VMS_ONLY(now_writer)));
			util_out_print("      ", FALSE);
			util_out_print("  Image_count           !12UL", TRUE, jb->image_count);
			util_out_print("  fsync_in_prog                !AD", FALSE, 5,
				       (jb->fsync_in_prog_latch.latch_pid ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  fsync pid             !12SL", TRUE, (jb->fsync_in_prog_latch.latch_pid));
			util_out_print("  fsync addrs             0x!8XL", FALSE, jb->fsync_dskaddr);
			util_out_print("      ", FALSE);
			util_out_print("  Need_db_fsync                !AD", TRUE, 5, (jb->need_db_fsync ? " TRUE" : "FALSE"));
			for (rectype = JRT_BAD + 1; rectype < JRT_RECTYPES - 1; rectype++)
			{
				util_out_print("  Jnl Rec Type    !5AZ      !7UL      ", FALSE, jrt_label[rectype],
					jb->reccnt[rectype]);
				rectype++;
				util_out_print("  Jnl Rec Type    !5AZ      !7UL", TRUE, jrt_label[rectype], jb->reccnt[rectype]);
			}
			if (rectype != JRT_RECTYPES)
				util_out_print("  Jnl Rec Type    !5AZ      !7UL", TRUE, jrt_label[rectype], jb->reccnt[rectype]);
		}
		util_out_print(0, TRUE);
		util_out_print("  Recover interrupted          !AD", FALSE, 5, (csd->recov_interrupted ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT resolve time   !12UL", TRUE, csd->intrpt_recov_tp_resolve_time);
		util_out_print("  INTRPT seqno    0x!16@XJ", FALSE, &csd->intrpt_recov_resync_seqno);
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT jnl_state      !12UL", TRUE, csd->intrpt_recov_jnl_state);
		util_out_print("  INTRPT repl_state     !12UL", FALSE, csd->intrpt_recov_repl_state);
		util_out_print(0, TRUE);
	}
	if (NEED_TO_DUMP("BACKUP") && (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb))
	{
		bptr = csa->backup_buffer;
		/* --------------------------- online backup buffer ---------------------------------- */
		util_out_print(0, TRUE);
		util_out_print("  Size                  !12UL", FALSE, bptr->size);
		util_out_print("      ", FALSE);
		util_out_print("  Free                  !12UL", TRUE, bptr->free);
		util_out_print("  Disk                  !12UL", FALSE, bptr->disk);
		util_out_print("      ", FALSE);
		util_out_print("  File Offset           !12UL", TRUE, bptr->dskaddr);
		util_out_print("  Io_in_progress(bckup) !12UL", FALSE, bptr->backup_ioinprog_latch.latch_pid ? TRUE : FALSE);
		util_out_print("      ", FALSE);
		util_out_print("  Backup_errno          !12UL", TRUE, bptr->backup_errno);
		util_out_print("  Backup Process ID     !12UL", FALSE, bptr->backup_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup TN             !12UL", TRUE, bptr->backup_tn);
		util_out_print("  Inc Backup TN         !12UL", FALSE, bptr->inc_backup_tn);
		util_out_print("      ", FALSE);
		util_out_print("  Process Failed        !12UL", TRUE, bptr->failed);
#if defined(VMS)
		util_out_print("  Backup Image Count    !12UL", TRUE, bptr->backup_image_count);
#endif
		util_out_print("  Temp File:    !AD", TRUE, LEN_AND_STR(&bptr->tempfilename[0]));
	}
        return;
}
