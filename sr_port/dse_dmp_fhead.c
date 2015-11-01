/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
LITREF	char		*gtm_dbversion_table[];

#define SHOW_STAT(TEXT, VARIABLE)       if (0 != csd->VARIABLE##_cntr) 					\
	util_out_print(TEXT"  0x!8XL        Transaction =   0x!16@XJ", TRUE, (csd->VARIABLE##_cntr),	\
		(&csd->VARIABLE##_tn));

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
	unsigned char		util_buff[MAX_UTIL_LEN], buffer[MAXNUMLEN];
	int			util_len, rectype, time_len, index;
	uint4			jnl_status;
	enum jnl_state_codes	jnl_state;
	gds_file_id		zero_fid;
	mval			dollarh_mval, zdate_mval;
	char			dollarh_buffer[MAXNUMLEN], zdate_buffer[sizeof(DSE_DMP_TIME_FMT)];
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	shmpool_buff_hdr_ptr_t	bptr;

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
		util_out_print("  Access method                          !AD", FALSE,
			2, (csd->acc_meth == dba_mm) ? "MM" : "BG");
		util_out_print("  Global Buffers        !12UL", TRUE, csd->n_bts);
		util_out_print("  Reserved Bytes        !19UL", FALSE, csd->reserved_bytes);
		util_out_print("  Block size (in bytes) !12UL", TRUE, csd->blk_size);
		util_out_print("  Maximum record size   !19UL", FALSE, csd->max_rec_size);
		util_out_print("  Starting VBN          !12UL", TRUE, csd->start_vbn);
		util_out_print("  Maximum key size      !19UL", FALSE, csd->max_key_size);
		util_out_print("  Total blocks            0x!8XL", TRUE, csa->ti->total_blks);
		util_out_print("  Null subscripts              !AD", FALSE, 12,
			(csd->null_subs == ALWAYS) ? "      ALWAYS" : (csd->null_subs == ALLOWEXISTING) ? "    EXISTING" :
				"       NEVER" );
		util_out_print("  Free blocks             0x!8XL", TRUE, csa->ti->free_blocks);

		/*
		   NOTE: Currently Std Null Collation is the only entry in one line,
		   For 64bit TN project, when some other fields will be added, this can
		   be adjusted then - MM Oct 04
		*/
		util_out_print("  Standard Null Collation       !AD", FALSE, 11,
			(csd->std_null_coll) ? "       TRUE" : "      FALSE");
		util_out_print("  Free space              0x!8XL", TRUE, csd->free_space);
		util_out_print("  Last Record Backup     0x!16@XJ", FALSE, &csd->last_rec_backup);
		util_out_print ("  Extension Count       !12UL", TRUE, csd->extension_size);
		util_out_print("  Last Database Backup   0x!16@XJ", FALSE, &csd->last_com_backup);
		if (csd->bplmap > 0)
			util_out_print("  Number of local maps  !12UL", TRUE,
				(csa->ti->total_blks + csd->bplmap - 1) / csd->bplmap);
		else
			util_out_print("  Number of local maps            ??", TRUE);
		util_out_print("  Last Bytestream Backup 0x!16@XJ", FALSE, &csd->last_inc_backup);
		util_out_print("  Lock space              0x!8XL", TRUE, csd->lock_space_size/OS_PAGELET_SIZE);
		util_out_print("  In critical section            0x!8XL", FALSE, csa->nl->in_crit);
		util_out_print("  Timers pending        !12UL", TRUE, csa->nl->wcs_timers + 1);
		if (FROZEN_BY_ROOT == csd->freeze)
			util_out_print("  Cache freeze id            FROZEN BY ROOT", FALSE);
		else
			util_out_print("  Cache freeze id                0x!8XL", FALSE, (csd->freeze)? csd->freeze : 0);
		dse_puttime(csd->flush_time, "  Flush timer            !AD", TRUE);
		util_out_print("  Freeze match                   0x!8XL", FALSE, csd->image_count ? csd->image_count : 0);
		util_out_print("  Flush trigger         !12UL", TRUE, csd->flush_trigger);
		util_out_print("  Current transaction    0x!16@XJ", FALSE, &csa->ti->curr_tn);
		util_out_print("  No. of writes/flush   !12UL", TRUE, csd->n_wrt_per_flu);
		util_out_print("  Maximum TN             0x!16@XJ", FALSE, &csd->max_tn);
		if (GDSVLAST > csd->certified_for_upgrade_to)
			util_out_print("  Certified for Upgrade to        !AD", TRUE,
				LEN_AND_STR(gtm_dbversion_table[csd->certified_for_upgrade_to]));
		else	/* out of range so print hex */
			util_out_print("  Certified for Upgrade to 0x!8XL", TRUE, csd->certified_for_upgrade_to);
		util_out_print("  Maximum TN Warn        0x!16@XJ", FALSE, &csd->max_tn_warn);
		if (GDSVLAST > csd->desired_db_format)
			util_out_print("  Desired DB Format               !AD", TRUE,
				       LEN_AND_STR(gtm_dbversion_table[csd->desired_db_format]));
		else	/* out of range so print hex */
			util_out_print("  Desired DB Format       0x!8XL", TRUE, csd->desired_db_format);
		util_out_print("  Master Bitmap Size           !12UL", FALSE, csd->master_map_len / DISK_BLOCK_SIZE);
		util_out_print("  Blocks to Upgrade       0x!8XL", TRUE, csd->blks_to_upgrd);
		if (csd->def_coll)
		{
			util_out_print("  Default Collation     !19UL", FALSE, csd->def_coll);
			util_out_print("  Collation Version     !12UL", TRUE, csd->def_coll_ver);
		}
		util_out_print("  Create in progress           !AD", FALSE, 12, (csd->createinprogress) ?
			"        TRUE" : "       FALSE");

#  ifdef CNTR_WORD_32
		util_out_print("  Modified cache blocks !12UL", TRUE, csa->nl->wcs_active_lvl);
#  else
		util_out_print("  Modified cache blocks !12UW", TRUE, csa->nl->wcs_active_lvl);
#  endif

		util_out_print("  Reference count       !19UL", FALSE, csa->nl->ref_cnt);
		util_out_print("  Wait Disk             !12UL", TRUE, csd->wait_disk_space);
		util_out_print("  Journal State               !AD", (jnl_notallowed == jnl_state), 13,
				(jnl_notallowed != jnl_state) ?
				((jnl_state == jnl_closed) ? "          OFF"
				 : (jnl_buff_open ? "           ON" : "[inactive] ON")) : "     DISABLED");
		if (jnl_notallowed != jnl_state)
		{
			util_out_print("  Journal Before imaging       !AD", TRUE,
				5, (csd->jnl_before_image) ? " TRUE" : "FALSE");
			util_out_print("  Journal Allocation    !19UL", FALSE, csd->jnl_alq);
			util_out_print("  Journal Extension     !12UL", TRUE, csd->jnl_deq);
			util_out_print("  Journal Buffer Size   !19UL", FALSE, csd->jnl_buffer_size);
			util_out_print("  Journal Alignsize     !12UL", TRUE, csd->alignsize / DISK_BLOCK_SIZE);
			util_out_print("  Journal AutoSwitchLimit !17UL", FALSE, csd->autoswitchlimit);
			util_out_print("  Journal Epoch Interval!12UL", TRUE, EPOCH_SECOND2SECOND(csd->epoch_interval));
#ifdef UNIX
			util_out_print("  Journal Yield Limit   !19UL", FALSE, csd->yield_lmt);
			util_out_print("  Journal Sync IO              !AD", TRUE, 5,
				(csd->jnl_sync_io ? " TRUE" : "FALSE"));
#endif
			util_out_print("  Journal File: !AD", TRUE, JNL_LEN_STR(csd));
		}
		if (BACKUP_NOT_IN_PROGRESS != csa->nl->nbb)
			util_out_print("  Online Backup NBB     !19UL", TRUE, csa->nl->nbb);
		/* Mutex Stuff */
		util_out_print("  Mutex Hard Spin Count !19UL", FALSE, csd->mutex_spin_parms.mutex_hard_spin_count);
		util_out_print("  Mutex Sleep Spin Count!12UL", TRUE, csd->mutex_spin_parms.mutex_sleep_spin_count);
		util_out_print("  Mutex Spin Sleep Time !19UL", FALSE,
			(csd->mutex_spin_parms.mutex_spin_sleep_mask == 0) ?
				0 : (csd->mutex_spin_parms.mutex_spin_sleep_mask + 1));
		util_out_print("  KILLs in progress     !12UL", TRUE, csd->kill_in_prog);
		util_out_print("  Replication State           !AD", FALSE, 13,
			(csd->repl_state == repl_closed ? "          OFF"
			: (csd->repl_state == repl_open ? "           ON" : " [WAS_ON] OFF")));
#ifndef __vax
		util_out_print("  Region Seqno    0x!16@XJ", TRUE, &csd->reg_seqno);
		util_out_print("  Resync Seqno           0x!16@XJ", FALSE, &csd->resync_seqno);
#else
		util_out_print("  Region Seqno    0x00000000!8@XJ", TRUE, &csd->reg_seqno);
		util_out_print("  Resync Seqno           0x00000000!8@XJ", FALSE, &csd->resync_seqno);
#endif
		util_out_print("  Resync trans    0x!16@XJ", TRUE, &csd->resync_tn);
	}
	if (CLI_PRESENT == cli_present("ALL"))
	{	/* Only dump if -/ALL as if part of above display */
		util_out_print("  Blks Last Record Backup        0x!8XL", FALSE, csd->last_rec_bkup_last_blk);
		util_out_print("  Blks Last Stream Backup 0x!8XL", TRUE, csd->last_inc_bkup_last_blk);
		util_out_print("  Blks Last Comprehensive Backup 0x!8XL", FALSE, csd->last_com_bkup_last_blk);
		util_out_print("  DB Creation Version             !AD", TRUE,
			       LEN_AND_STR(gtm_dbversion_table[csd->creation_db_ver]));
	}
	if (NEED_TO_DUMP("ENVIRONMENT"))
	{
                util_out_print(0, TRUE);
		util_out_print("  Full Block Writes                  !AD", FALSE, 6,
			(csa->do_fullblockwrites) ? "    ON" : "   OFF");
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
                util_out_print(0, TRUE);
		for (index = 0; index <= sizeof(csd->n_tp_retries)/sizeof(csd->n_tp_retries[0]); index++)
			util_out_print("  Total TP Retries [!2UL] !12UL     Cnflct TP Retries [!2UL] !12UL",
				TRUE, index, csd->n_tp_retries[index], index, csd->n_tp_retries_conflicts[index]);
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
                util_out_print("  TP blkmod nomod       !12UL", TRUE, csd->tp_cdb_sc_blkmod[tp_blkmod_nomod]);
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
#include "tab_bg_trc_rec.h"
#undef TAB_BG_TRC_REC
	}
	jpc = csa->jnl;
	if (NEED_TO_DUMP("JOURNAL") && (JNL_ENABLED(csd) && (NULL != jpc) && (NULL != jpc->jnl_buff)))
	{
		jb = jpc->jnl_buff;
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
		util_out_print("  Journal checksum seed   0x!8XL", FALSE, csd->jnl_checksum);
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
		util_out_print("  Epoch_tn        0x!16@XJ", TRUE, &jb->epoch_tn);
		util_out_print("  Io_in_progress               !AD", FALSE, 5,
		  (jb->UNIX_ONLY(io_in_prog_latch.u.parts.latch_pid)VMS_ONLY(io_in_prog) ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  Epoch_Interval        !12UL", TRUE, EPOCH_SECOND2SECOND(jb->epoch_interval));
		util_out_print("  Now_writer            !12UL", FALSE,
			       (jb->UNIX_ONLY(io_in_prog_latch.u.parts.latch_pid)VMS_ONLY(now_writer)));
		util_out_print("      ", FALSE);
		util_out_print("  Image_count           !12UL", TRUE, jb->image_count);
		util_out_print("  fsync_in_prog                !AD", FALSE, 5,
			       (jb->fsync_in_prog_latch.u.parts.latch_pid ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  fsync pid             !12SL", TRUE, (jb->fsync_in_prog_latch.u.parts.latch_pid));
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
	if (NEED_TO_DUMP("BACKUP"))
	{
		bptr = csa->shmpool_buffer;
		/* --------------------------- online backup buffer ---------------------------------- */
		util_out_print(0, TRUE);
		util_out_print("  Free blocks           !12UL", FALSE, bptr->free_cnt);
		util_out_print("      ", FALSE);
		util_out_print("  Backup blocks         !12UL", TRUE, bptr->backup_cnt);
		util_out_print("  Reformat blocks       !12UL", FALSE, bptr->reformat_cnt);
		util_out_print("      ", FALSE);
		util_out_print("  Total blocks          !12UL", TRUE, bptr->total_blks);
		util_out_print("  Shmpool blocked              !AD", FALSE, 5, (bptr->shmpool_blocked ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  File Offset     0x!16@XJ", TRUE, &bptr->dskaddr);
		util_out_print("  Shmpool crit holder   !12UL", FALSE, bptr->shmpool_crit_latch.u.parts.latch_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup_errno          !12UL", TRUE, bptr->backup_errno);
#ifdef VMS
		util_out_print("  Shmpool crit imgcnt   !12UL", TRUE, bptr->shmpool_crit_latch.u.parts.latch_image_count);
#endif
		util_out_print("  Backup Process ID     !12UL", FALSE, bptr->backup_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup TN       0x!16@XJ", TRUE, &bptr->backup_tn);
		util_out_print("  Inc Backup TN   0x!16@XJ", FALSE, &bptr->inc_backup_tn);
		util_out_print("      ", FALSE);
		util_out_print("  Process Failed        !12UL", TRUE, bptr->failed);
		util_out_print("  Allocs since check    !12UL", FALSE, bptr->allocs_since_chk);
		util_out_print("      ", FALSE);
		util_out_print("  Backup Image Count    !12UL", TRUE, bptr->backup_image_count);
		util_out_print("  Temp File:    !AD", TRUE, LEN_AND_STR(&bptr->tempfilename[0]));
	}
	if (NEED_TO_DUMP("MIXEDMODE"))
	{
		util_out_print(0, TRUE);
		util_out_print("  Database is Fully Upgraded                : !AD",
			TRUE, 5, (csd->fully_upgraded ? " TRUE" : "FALSE"));
		util_out_print("  Blocks to Upgrade subzero(negative) error : 0x!8XL", TRUE, csd->blks_to_upgrd_subzero_error);
		util_out_print("  TN when Blocks to Upgrade last became 0   : 0x!16@XJ", TRUE, &csd->tn_upgrd_blks_0);
		util_out_print("  TN when Desired DB Format last changed    : 0x!16@XJ", TRUE, &csd->desired_db_format_tn);
		util_out_print("  TN when REORG upgrd/dwngrd changed dbfmt  : 0x!16@XJ", TRUE, &csd->reorg_db_fmt_start_tn);
		util_out_print(0, TRUE);
		util_out_print("  Block Number REORG upgrd/dwngrd will restart from : 0x!8XL",
			TRUE, csd->reorg_upgrd_dwngrd_restart_block);
	}
	if (NEED_TO_DUMP("UPDPROC"))
	{
		util_out_print(0, TRUE);
		util_out_print("  Upd reserved area [% global buffers]  !3UL", FALSE, csd->reserved_for_upd);
		util_out_print("  Avg blks read per 100 records !4UL", TRUE, csd->avg_blks_per_100gbl);
		util_out_print("  Pre read trigger factor [% upd rsrvd] !3UL", FALSE, csd->pre_read_trigger_factor);
		util_out_print("  Upd writer trigger [%flshTrgr] !3UL", TRUE, csd->writer_trigger_factor);
	}
        return;
}
