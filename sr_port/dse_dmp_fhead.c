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

#define SHOW_STAT(TEXT, VARIABLE)       if (0 != cs_addrs->hdr->VARIABLE.evnt_cnt) 					\
	util_out_print(TEXT"  0x!8XL        Transaction =           0x!8XL", TRUE, (cs_addrs->hdr->VARIABLE.evnt_cnt),	\
		(cs_addrs->hdr->VARIABLE.evnt_tn));

#define SHOW_DB_CSH_STAT(COUNTER, TEXT1, TEXT2)									\
	if (cs_addrs->hdr->COUNTER.curr_count || cs_addrs->hdr->COUNTER.cumul_count)				\
	{													\
		util_out_print(TEXT1"  0x!8XL      "TEXT2"  0x!8XL", TRUE, (cs_addrs->hdr->COUNTER.curr_count), \
				(cs_addrs->hdr->COUNTER.cumul_count + cs_addrs->hdr->COUNTER.curr_count));	\
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

        jnl_state = (uint4)cs_addrs->hdr->jnl_state;
	VMS_ONLY(
		memset(&zero_fid, 0, sizeof(zero_fid));
		jnl_buff_open = (0 != memcmp(cs_addrs->nl->jnl_file.jnl_file_id.fid, zero_fid.fid, sizeof(zero_fid.fid)));
	)
	UNIX_ONLY(
		jnl_buff_open = (0 != cs_addrs->nl->jnl_file.u.inode);
	)
	if (CLI_NEGATED != cli_present("BASIC"))
	{
		util_out_print("!/File            !AD", TRUE, gv_cur_region->dyn.addr->fname_len,
			&gv_cur_region->dyn.addr->fname[0]);
		util_out_print("Region          !AD", TRUE, gv_cur_region->rname_len, &gv_cur_region->rname[0]);
		GET_CURR_TIME_IN_DOLLARH_AND_ZDATE(dollarh_mval, dollarh_buffer, zdate_mval, zdate_buffer);
		util_out_print("Date/Time       !AD [$H = !AD]", TRUE, zdate_mval.str.len, zdate_mval.str.addr,
				dollarh_mval.str.len, dollarh_mval.str.addr);
		util_out_print("  Access method                   !AD", FALSE, 2,
			(cs_addrs->hdr->acc_meth == dba_mm) ? "MM" : "BG");
		util_out_print("      ", FALSE);
		util_out_print("  Global Buffers        !12UL", TRUE, cs_addrs->hdr->n_bts);
		util_out_print("  Reserved Bytes        !12UL", FALSE, cs_addrs->hdr->reserved_bytes);
		util_out_print("      ", FALSE);
		util_out_print("  Block size (in bytes) !12UL", TRUE, cs_addrs->hdr->blk_size);
		util_out_print("  Maximum record size   !12UL", FALSE, cs_addrs->hdr->max_rec_size);
		util_out_print("      ", FALSE);
		util_out_print("  Starting VBN          !12UL", TRUE, cs_addrs->hdr->start_vbn);
		util_out_print("  Maximum key size      !12UL", FALSE, cs_addrs->hdr->max_key_size);
		util_out_print("      ", FALSE);
		util_out_print("  Total blocks            0x!8XL", TRUE, cs_addrs->ti->total_blks);
		util_out_print("  Null subscripts       !AD", FALSE, 12,
			(cs_addrs->hdr->null_subs) ? "        TRUE" : "       FALSE");
		util_out_print("      ", FALSE);
		util_out_print("  Free blocks             0x!8XL", TRUE, cs_addrs->ti->free_blocks);
		util_out_print("  Last Record Backup      0x!8XL", FALSE, cs_addrs->hdr->last_rec_backup);
		util_out_print("      ", FALSE);
		util_out_print ("  Extension Count       !12UL", TRUE, cs_addrs->hdr->extension_size);
		util_out_print("  Last Database Bckup     0x!8XL", FALSE, cs_addrs->hdr->last_com_backup);
		util_out_print("      ", FALSE);
		if (cs_addrs->hdr->bplmap > 0)
			util_out_print("  Number of local maps  !12UL", TRUE,
				(cs_addrs->ti->total_blks + cs_addrs->hdr->bplmap - 1) / cs_addrs->hdr->bplmap);
		else
			util_out_print("  Number of local maps            ??", TRUE);
		util_out_print("  Last Bytestream Bckup   0x!8XL", FALSE, cs_addrs->hdr->last_inc_backup);
		util_out_print("      ", FALSE);
		util_out_print("  Lock space              0x!8XL", TRUE, cs_addrs->hdr->lock_space_size/OS_PAGELET_SIZE);
		util_out_print("  In critical section     0x!8XL", FALSE, cs_addrs->nl->in_crit);
		util_out_print("      ", FALSE);
		util_out_print("  Timers pending        !12UL", TRUE, cs_addrs->nl->wcs_timers + 1);
		if (FROZEN_BY_ROOT == cs_addrs->hdr->freeze)
			util_out_print("  Cache freeze id     FROZEN BY ROOT", FALSE);
		else
			util_out_print("  Cache freeze id         0x!8XL", FALSE,
				(cs_addrs->hdr->freeze)? cs_addrs->hdr->freeze : 0);
		util_out_print("      ", FALSE);
		dse_puttime(cs_addrs->hdr->flush_time, "  Flush timer            !AD", TRUE);
		util_out_print("  Freeze match            0x!8XL", FALSE,
			(cs_addrs->hdr->image_count)? cs_addrs->hdr->image_count : 0);
		util_out_print("      ", FALSE);
		util_out_print("  Flush trigger         !12UL", TRUE, cs_addrs->hdr->flush_trigger);
		util_out_print("  Current transaction     0x!8XL", FALSE, cs_addrs->ti->curr_tn);
		util_out_print("      ", FALSE);
		util_out_print("  No. of writes/flush   !12UL", TRUE, cs_addrs->hdr->n_wrt_per_flu);
		if (cs_addrs->hdr->def_coll)
		{
			util_out_print("  Default Collation     !12UL", FALSE, cs_addrs->hdr->def_coll);
			util_out_print("      ", FALSE);
			util_out_print("  Collation Version     !12UL", TRUE, cs_addrs->hdr->def_coll_ver);
		}
		util_out_print("  Create in progress    !AD", FALSE, 12,
			(cs_addrs->hdr->createinprogress) ? "        TRUE" : "       FALSE");
		util_out_print("      ", FALSE);

#  ifdef CNTR_WORD_32
		util_out_print("  Modified cache blocks !12UL", TRUE, cs_addrs->nl->wcs_active_lvl);
#  else
		util_out_print("  Modified cache blocks !12UW", TRUE, cs_addrs->nl->wcs_active_lvl);
#  endif

		util_out_print("  Reference count       !12UL", FALSE, cs_addrs->nl->ref_cnt);
		util_out_print("      ", FALSE);
		util_out_print("  Wait Disk             !12UL", TRUE, cs_addrs->hdr->wait_disk_space);
		util_out_print("  Journal State        !AD", (jnl_notallowed == jnl_state), 13,
				(jnl_notallowed != jnl_state) ?
				((jnl_state == jnl_closed) ? "          OFF" :
				 	(jnl_buff_open ? "           ON" : "[inactive] ON")) : "     DISABLED");
		if (jnl_notallowed != jnl_state)
		{
			util_out_print("      ", FALSE);
			util_out_print("  Journal Before imaging       !AD", TRUE, 5,
				(cs_addrs->hdr->jnl_before_image) ? " TRUE" : "FALSE");
			util_out_print("  Journal Allocation    !12UL", FALSE, cs_addrs->hdr->jnl_alq);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Extension     !12UL", TRUE, cs_addrs->hdr->jnl_deq);
			util_out_print("  Journal Buffer Size   !12UL", FALSE, cs_addrs->hdr->jnl_buffer_size);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Alignsize     !12UL", TRUE, cs_addrs->hdr->alignsize / DISK_BLOCK_SIZE);
			util_out_print("  Journal AutoSwitchLimit !10UL", FALSE, cs_addrs->hdr->autoswitchlimit);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Epoch Interval!12UL", TRUE, EPOCH_SECOND2SECOND(cs_addrs->hdr->epoch_interval));
#ifdef UNIX
			util_out_print("  Journal Yield Limit   !12UL", FALSE, cs_addrs->hdr->yield_lmt);
			util_out_print("      ", FALSE);
			util_out_print("  Journal Sync IO              !AD", TRUE, 5,
					(cs_addrs->hdr->jnl_sync_io ? " TRUE" : "FALSE"));
#endif
			util_out_print("  Journal File: !AD", TRUE, JNL_LEN_STR(cs_addrs->hdr));
		}
		if (BACKUP_NOT_IN_PROGRESS != cs_addrs->nl->nbb)
			util_out_print("  Online Backup NBB     !12UL", TRUE, cs_addrs->nl->nbb);
		/* Mutex Stuff */
		util_out_print("  Mutex Hard Spin Count !12UL", FALSE, cs_addrs->hdr->mutex_spin_parms.mutex_hard_spin_count);
		util_out_print("      ", FALSE);
		util_out_print("  Mutex Sleep Spin Count!12UL", TRUE, cs_addrs->hdr->mutex_spin_parms.mutex_sleep_spin_count);
		util_out_print("  Mutex Spin Sleep Time !12UL", FALSE,
			(cs_addrs->hdr->mutex_spin_parms.mutex_spin_sleep_mask == 0) ?
				0 : (cs_addrs->hdr->mutex_spin_parms.mutex_spin_sleep_mask + 1));
		util_out_print("      ", FALSE);
		util_out_print("  KILLs in progress     !12UL", TRUE, cs_addrs->hdr->kill_in_prog);
		util_out_print("  Replication State           !AD", FALSE, 6,
			(cs_addrs->hdr->repl_state == repl_closed)? "   OFF" : "    ON");
		ptr = i2asclx(buffer, cs_addrs->hdr->reg_seqno);
#ifndef __vax
		util_out_print("        Region Seqno    0x!16@XJ", TRUE, &cs_addrs->hdr->reg_seqno);
		util_out_print("  Resync Seqno    0x!16@XJ", FALSE, &cs_addrs->hdr->resync_seqno);
#else
		util_out_print("        Region Seqno    0x00000000!8@XJ", TRUE, &cs_addrs->hdr->reg_seqno);
		util_out_print("  Resync Seqno    0x00000000!8@XJ", FALSE, &cs_addrs->hdr->resync_seqno);
#endif
		util_out_print("        Resync transaction      0x!8XL", TRUE, cs_addrs->hdr->resync_tn);
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
                util_out_print("  Retries [0]           !12UL", TRUE, cs_addrs->hdr->n_retries[0]);
                util_out_print("  Retries [1]           !12UL", TRUE, cs_addrs->hdr->n_retries[1]);
                util_out_print("  Retries [2]           !12UL", TRUE, cs_addrs->hdr->n_retries[2]);
                util_out_print("  Retries [3]           !12UL", TRUE, cs_addrs->hdr->n_retries[3]);
	}
	if (NEED_TO_DUMP("TPRETRIES"))
	{
#define	DUMP_TP_RETRIES(X)										\
	util_out_print("  Total TP Retries ["#X"]  !12UL        Cnflct TP Retries ["#X"] !12UL",	\
			TRUE, cs_addrs->hdr->n_tp_retries[X], cs_addrs->hdr->n_tp_retries_conflicts[X]);
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
	if (NEED_TO_DUMP("TPBLKMOD"))
	{
                util_out_print(0, TRUE);
		assert(n_tp_blkmod_types < ARRAYSIZE(cs_addrs->hdr->tp_cdb_sc_blkmod));
                util_out_print("  TP blkmod gvcst_put   !12UL", TRUE, cs_addrs->hdr->tp_cdb_sc_blkmod[tp_blkmod_gvcst_put]);
                util_out_print("  TP blkmod gvcst_srch  !12UL", TRUE, cs_addrs->hdr->tp_cdb_sc_blkmod[tp_blkmod_gvcst_srch]);
                util_out_print("  TP blkmod t_qread     !12UL", TRUE, cs_addrs->hdr->tp_cdb_sc_blkmod[tp_blkmod_t_qread]);
                util_out_print("  TP blkmod tp_tend     !12UL", TRUE, cs_addrs->hdr->tp_cdb_sc_blkmod[tp_blkmod_tp_tend]);
                util_out_print("  TP blkmod tp_hist     !12UL", TRUE, cs_addrs->hdr->tp_cdb_sc_blkmod[tp_blkmod_tp_hist]);
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
	if (NEED_TO_DUMP("JOURNAL") && (JNL_ENABLED(cs_addrs->hdr) && (NULL != cs_addrs->jnl) && (NULL != cs_addrs->jnl->jnl_buff)))
	{
		if (jnl_buff_open)
		{
			util_out_print(0, TRUE);
			/* --------------------------- journal buffer --------------------------------- */
			util_out_print("  Jnl Buffer Size       !12UL", FALSE, cs_addrs->jnl->jnl_buff->size);
			util_out_print("      ", FALSE);
			util_out_print("  Dskaddr               !12UL", TRUE, cs_addrs->jnl->jnl_buff->dskaddr);
			util_out_print("  Free                  !12UL", FALSE, cs_addrs->jnl->jnl_buff->free);
			util_out_print("      ", FALSE);
			util_out_print("  Freeaddr              !12UL", TRUE, cs_addrs->jnl->jnl_buff->freeaddr);
			util_out_print("  Dsk                   !12UL", FALSE, cs_addrs->jnl->jnl_buff->dsk);
			util_out_print("      ", FALSE);
			util_out_print("  Wrtsize               !12UL", FALSE, cs_addrs->jnl->jnl_buff->wrtsize);
			util_out_print("      ", FALSE);
			util_out_print("  Min_write_size        !12UL", TRUE, cs_addrs->jnl->jnl_buff->min_write_size);
			util_out_print("  bytcnt                !12UL", FALSE, cs_addrs->jnl->jnl_buff->bytcnt);
			util_out_print("      ", FALSE);
			util_out_print("  Max_write_size        !12UL", TRUE, cs_addrs->jnl->jnl_buff->max_write_size);
			util_out_print("  Before image                 !AD", FALSE, 5,
				(cs_addrs->jnl->jnl_buff->before_images ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  Filesize              !12UL", TRUE, cs_addrs->jnl->jnl_buff->filesize);
			util_out_print("  Iosb.cond             !12UW", FALSE, cs_addrs->jnl->jnl_buff->iosb.cond);
			util_out_print("      ", FALSE);
			util_out_print("  qiocnt                !12UL", TRUE, cs_addrs->jnl->jnl_buff->qiocnt);
			util_out_print("  Iosb.length           !12UW", FALSE, cs_addrs->jnl->jnl_buff->iosb.length);
			util_out_print("      ", FALSE);
			util_out_print("  errcnt                !12UL", TRUE, cs_addrs->jnl->jnl_buff->errcnt);
			util_out_print("  Iosb.dev_specific     !12UL", FALSE, cs_addrs->jnl->jnl_buff->iosb.dev_specific);
			util_out_print("      ", FALSE);
			time_len = exttime(cs_addrs->jnl->jnl_buff->next_epoch_time, (char *)buffer, 0);
			assert(STR_LIT_LEN(NEXT_EPOCH_TIME_SPACES) >= time_len);
			util_out_print("  Next Epoch_Time!AD!AD", TRUE, STR_LIT_LEN(NEXT_EPOCH_TIME_SPACES) - time_len + 1,
					NEXT_EPOCH_TIME_SPACES, time_len - 1, buffer); /* -1 to avoid printing \ at end of $H
											* format time returned by exttime */
			util_out_print("  Blocked Process       !12UL", FALSE, cs_addrs->jnl->jnl_buff->blocked);
			util_out_print("      ", FALSE);
			util_out_print("  Epoch_tn                0x!8XL", TRUE, cs_addrs->jnl->jnl_buff->epoch_tn);
			util_out_print("  Io_in_progress               !AD", FALSE, 5,
			  (cs_addrs->jnl->jnl_buff->UNIX_ONLY(io_in_prog_latch.latch_pid)VMS_ONLY(io_in_prog) ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  Epoch_Interval        !12UL", TRUE,
					EPOCH_SECOND2SECOND(cs_addrs->jnl->jnl_buff->epoch_interval));
			util_out_print("  Now_writer            !12UL", FALSE,
				       (cs_addrs->jnl->jnl_buff->UNIX_ONLY(io_in_prog_latch.latch_pid)VMS_ONLY(now_writer)));
			util_out_print("      ", FALSE);
			util_out_print("  Image_count           !12UL", TRUE, cs_addrs->jnl->jnl_buff->image_count);
			util_out_print("  fsync_in_prog                !AD", FALSE, 5,
				       (cs_addrs->jnl->jnl_buff->fsync_in_prog_latch.latch_pid ? " TRUE" : "FALSE"));
			util_out_print("      ", FALSE);
			util_out_print("  fsync pid             !12SL", TRUE,
				       (cs_addrs->jnl->jnl_buff->fsync_in_prog_latch.latch_pid));
			util_out_print("  fsync addrs             0x!8XL", FALSE, cs_addrs->jnl->jnl_buff->fsync_dskaddr);
			util_out_print("      ", FALSE);
			util_out_print("  Need_db_fsync                !AD", TRUE, 5,
				(cs_addrs->jnl->jnl_buff->need_db_fsync ? " TRUE" : "FALSE"));
			for (rectype = JRT_BAD + 1; rectype < JRT_RECTYPES - 1; rectype++)
			{
				util_out_print("  Jnl Rec Type    !5AZ      !7UL      ", FALSE, jrt_label[rectype],
					cs_addrs->jnl->jnl_buff->reccnt[rectype]);
				rectype++;
				util_out_print("  Jnl Rec Type    !5AZ      !7UL", TRUE, jrt_label[rectype],
					cs_addrs->jnl->jnl_buff->reccnt[rectype]);
			}
			if (rectype != JRT_RECTYPES)
			{
				util_out_print("  Jnl Rec Type    !5AZ      !7UL", TRUE, jrt_label[rectype],
					cs_addrs->jnl->jnl_buff->reccnt[rectype]);
			}
		}
		util_out_print(0, TRUE);
		util_out_print("  Recover interrupted          !AD", FALSE, 5,
			(cs_addrs->hdr->recov_interrupted ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT resolve time   !12UL", TRUE, cs_addrs->hdr->intrpt_recov_tp_resolve_time);
		util_out_print("  INTRPT seqno    0x!16@XJ", FALSE, &cs_addrs->hdr->intrpt_recov_resync_seqno);
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT jnl_state      !12UL", TRUE, cs_addrs->hdr->intrpt_recov_jnl_state);
		util_out_print("  INTRPT repl_state     !12UL", FALSE, cs_addrs->hdr->intrpt_recov_repl_state);
		util_out_print(0, TRUE);
	}
	if (NEED_TO_DUMP("BACKUP") && (BACKUP_NOT_IN_PROGRESS != cs_addrs->nl->nbb))
	{
		/* --------------------------- online backup buffer ---------------------------------- */
		util_out_print(0, TRUE);
		util_out_print("  Size                  !12UL", FALSE, cs_addrs->backup_buffer->size);
		util_out_print("      ", FALSE);
		util_out_print("  Free                  !12UL", TRUE, cs_addrs->backup_buffer->free);
		util_out_print("  Disk                  !12UL", FALSE, cs_addrs->backup_buffer->disk);
		util_out_print("      ", FALSE);
		util_out_print("  File Offset           !12UL", TRUE, cs_addrs->backup_buffer->dskaddr);
		util_out_print("  Io_in_progress(bckup) !12UL", FALSE,
			       cs_addrs->backup_buffer->backup_ioinprog_latch.latch_pid ? TRUE : FALSE);
		util_out_print("      ", FALSE);
		util_out_print("  Backup_errno          !12UL", TRUE, cs_addrs->backup_buffer->backup_errno);
		util_out_print("  Backup Process ID     !12UL", FALSE, cs_addrs->backup_buffer->backup_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup TN             !12UL", TRUE, cs_addrs->backup_buffer->backup_tn);
		util_out_print("  Inc Backup TN         !12UL", FALSE, cs_addrs->backup_buffer->inc_backup_tn);
		util_out_print("      ", FALSE);
		util_out_print("  Process Failed        !12UL", TRUE, cs_addrs->backup_buffer->failed);
#if defined(VMS)
		util_out_print("  Backup Image Count    !12UL", TRUE, cs_addrs->backup_buffer->backup_image_count);
#endif
		util_out_print("  Temp File:    !AD", TRUE, strlen(&cs_addrs->backup_buffer->tempfilename[0]),
							&cs_addrs->backup_buffer->tempfilename[0]);
	}
        return;
}
