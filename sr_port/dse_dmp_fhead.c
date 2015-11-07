/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "shmpool.h"	/* Needed for the shmpool structures */
#ifdef GTM_SNAPSHOT
#include "db_snapshot.h"
#endif

#define MAX_UTIL_LEN    	64
#define NEXT_EPOCH_TIME_SPACES	"                   " /* 19 spaces, we have 19 character field width to output Next Epoch Time */

GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	gd_region	*gv_cur_region;
GBLREF	boolean_t	dse_all_dump;		/* TRUE if DSE ALL -DUMP is specified */

GBLDEF mval dse_dmp_time_fmt = DEFINE_MVAL_STRING(MV_STR, 0, 0, STR_LIT_LEN(DSE_DMP_TIME_FMT), DSE_DMP_TIME_FMT, 0, 0);

LITREF	char		*jrt_label[JRT_RECTYPES];
LITREF	char		*gtm_dbversion_table[];


#define SHOW_STAT(TEXT, VARIABLE)       if (0 != csd->VARIABLE##_cntr) 					\
	util_out_print(TEXT"  0x!XL        Transaction =   0x!16@XQ", TRUE, (csd->VARIABLE##_cntr),	\
		(&csd->VARIABLE##_tn));

#define SHOW_DB_CSH_STAT(csd, COUNTER, TEXT1, TEXT2)							\
	if (csd->COUNTER.curr_count || csd->COUNTER.cumul_count)					\
	{												\
		util_out_print(TEXT1"  0x!XL      "TEXT2"  0x!XL", TRUE, (csd->COUNTER.curr_count),	\
				(csd->COUNTER.cumul_count + csd->COUNTER.curr_count));			\
	}

#define SHOW_GVSTATS_STAT(cnl, COUNTER, TEXT1, TEXT2)							\
{													\
	if (cnl->gvstats_rec.COUNTER)									\
		util_out_print("  " TEXT1 " : " TEXT2"  0x!16@XQ", TRUE, (&cnl->gvstats_rec.COUNTER));	\
}

/* NEED_TO_DUMP is only for the qualifiers other than "BASIC" and "ALL".
	file_header is not dumped only if "NOBASIC" is explicitly specified */

#define	NEED_TO_DUMP(string)													\
	(is_dse_all ? (CLI_PRESENT == cli_present("ALL"))									\
		: (CLI_PRESENT == cli_present(string) || CLI_PRESENT == cli_present("ALL") && CLI_NEGATED != cli_present(string)))

void dse_dmp_fhead (void)
{
	boolean_t		jnl_buff_open;
	unsigned char		util_buff[MAX_UTIL_LEN], buffer[MAXNUMLEN];
	int			util_len, rectype, time_len, index;
	uint4			jnl_status;
	enum jnl_state_codes	jnl_state;
	gds_file_id		zero_fid;
	mval			dollarh_mval, zdate_mval;
	char			dollarh_buffer[MAXNUMLEN], zdate_buffer[SIZEOF(DSE_DMP_TIME_FMT)];
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	node_local_ptr_t	cnl;
	jnl_private_control	*jpc;
	jnl_buffer_ptr_t	jb;
	shmpool_buff_hdr_ptr_t	bptr;
	boolean_t		is_dse_all;
	uint4			pid;
	boolean_t		new_line;
	unsigned char		outbuf[GTMCRYPT_HASH_HEX_LEN + 1];
	GTM_SNAPSHOT_ONLY(
		shm_snapshot_t	*ss_shm_ptr;)

	is_dse_all = dse_all_dump;
	dse_all_dump = FALSE;
	csa = cs_addrs;
	csd = csa->hdr;
	cnl = csa->nl;
        jnl_state = (enum jnl_state_codes)csd->jnl_state;
	VMS_ONLY(
		memset(&zero_fid, 0, SIZEOF(zero_fid));
		jnl_buff_open = (0 != memcmp(cnl->jnl_file.jnl_file_id.fid, zero_fid.fid, SIZEOF(zero_fid.fid)));
	)
	UNIX_ONLY(
		jnl_buff_open = (0 != cnl->jnl_file.u.inode);
	)
	if (is_dse_all || (CLI_NEGATED != cli_present("BASIC")))
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
		util_out_print("  Total blocks            0x!XL", TRUE, csa->ti->total_blks);
		util_out_print("  Null subscripts              !AD", FALSE, 12,
			(csd->null_subs == ALWAYS) ? "      ALWAYS" : (csd->null_subs == ALLOWEXISTING) ? "    EXISTING" :
				"       NEVER" );
		util_out_print("  Free blocks             0x!XL", TRUE, csa->ti->free_blocks);

		/*
		   NOTE: Currently Std Null Collation is the only entry in one line,
		   For 64bit TN project, when some other fields will be added, this can
		   be adjusted then - MM Oct 04
		*/
		util_out_print("  Standard Null Collation       !AD", FALSE, 11,
			(csd->std_null_coll) ? "       TRUE" : "      FALSE");
		util_out_print("  Free space              0x!XL", TRUE, csd->free_space);
		util_out_print("  Last Record Backup     0x!16@XQ", FALSE, &csd->last_rec_backup);
		util_out_print ("  Extension Count       !12UL", TRUE, csd->extension_size);
		util_out_print("  Last Database Backup   0x!16@XQ", FALSE, &csd->last_com_backup);
		if (csd->bplmap > 0)
			util_out_print("  Number of local maps  !12UL", TRUE,
				(csa->ti->total_blks + csd->bplmap - 1) / csd->bplmap);
		else
			util_out_print("  Number of local maps            ??", TRUE);
		util_out_print("  Last Bytestream Backup 0x!16@XQ", FALSE, &csd->last_inc_backup);
		util_out_print("  Lock space              0x!XL", TRUE, csd->lock_space_size/OS_PAGELET_SIZE);
		util_out_print("  In critical section            0x!XL", FALSE, cnl->in_crit);
		util_out_print("  Timers pending        !12UL", TRUE, cnl->wcs_timers + 1);
		if (FROZEN_BY_ROOT == csd->freeze)
			util_out_print("  Cache freeze id            FROZEN BY ROOT", FALSE);
		else
			util_out_print("  Cache freeze id                0x!XL", FALSE, (csd->freeze)? csd->freeze : 0);
		dse_puttime(csd->flush_time, "  Flush timer            !AD", TRUE);
		util_out_print("  Freeze match                   0x!XL", FALSE, csd->image_count ? csd->image_count : 0);
		util_out_print("  Flush trigger         !12UL", TRUE, csd->flush_trigger);
		util_out_print("  Current transaction    0x!16@XQ", FALSE, &csa->ti->curr_tn);
		util_out_print("  No. of writes/flush   !12UL", TRUE, csd->n_wrt_per_flu);
		util_out_print("  Maximum TN             0x!16@XQ", FALSE, &csd->max_tn);
		if (GDSVLAST > csd->certified_for_upgrade_to)
			util_out_print("  Certified for Upgrade to        !AD", TRUE,
				LEN_AND_STR(gtm_dbversion_table[csd->certified_for_upgrade_to]));
		else	/* out of range so print hex */
			util_out_print("  Certified for Upgrade to 0x!XL", TRUE, csd->certified_for_upgrade_to);
		util_out_print("  Maximum TN Warn        0x!16@XQ", FALSE, &csd->max_tn_warn);
		if (GDSVLAST > csd->desired_db_format)
			util_out_print("  Desired DB Format               !AD", TRUE,
				       LEN_AND_STR(gtm_dbversion_table[csd->desired_db_format]));
		else	/* out of range so print hex */
			util_out_print("  Desired DB Format       0x!XL", TRUE, csd->desired_db_format);
		util_out_print("  Master Bitmap Size           !12UL", FALSE, csd->master_map_len / DISK_BLOCK_SIZE);
		util_out_print("  Blocks to Upgrade       0x!XL", TRUE, csd->blks_to_upgrd);
		if (csd->def_coll)
		{
			util_out_print("  Default Collation     !19UL", FALSE, csd->def_coll);
			util_out_print("  Collation Version     !12UL", TRUE, csd->def_coll_ver);
		}
		util_out_print("  Create in progress           !AD", FALSE, 12, (csd->createinprogress) ?
			"        TRUE" : "       FALSE");

#		ifdef CNTR_WORD_32
		util_out_print("  Modified cache blocks !12UL", TRUE, cnl->wcs_active_lvl);
#		else
		util_out_print("  Modified cache blocks !12UW", TRUE, cnl->wcs_active_lvl);
#		endif

		util_out_print("  Reference count       !19UL", FALSE, cnl->ref_cnt);
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
#			ifdef UNIX
			util_out_print("  Journal Yield Limit   !19UL", FALSE, csd->yield_lmt);
			util_out_print("  Journal Sync IO              !AD", TRUE, 5,
				(csd->jnl_sync_io ? " TRUE" : "FALSE"));
#			elif VMS
			util_out_print("  Journal NOCACHE IO           !AD", TRUE, 12,
				(csd->jnl_sync_io ? "        TRUE" : "       FALSE"));
#			endif
			util_out_print("  Journal File: !AD", TRUE, JNL_LEN_STR(csd));
		}
		if (BACKUP_NOT_IN_PROGRESS != cnl->nbb)
			util_out_print("  Online Backup NBB     !19UL", TRUE, cnl->nbb);
		/* Mutex Stuff */
		util_out_print("  Mutex Hard Spin Count !19UL", FALSE, csd->mutex_spin_parms.mutex_hard_spin_count);
		util_out_print("  Mutex Sleep Spin Count!12UL", TRUE, csd->mutex_spin_parms.mutex_sleep_spin_count);
		util_out_print("  Mutex Queue Slots     !19UL", FALSE, NUM_CRIT_ENTRY(csd));
		util_out_print("  KILLs in progress     !12UL", TRUE, (csd->kill_in_prog + csd->abandoned_kills));
		util_out_print("  Replication State           !AD", FALSE, 13,
			(csd->repl_state == repl_closed ? "          OFF"
			: (csd->repl_state == repl_open ? "           ON" : " [WAS_ON] OFF")));
		util_out_print("  Region Seqno    0x!16@XQ", TRUE, &csd->reg_seqno);
		VMS_ONLY(
			util_out_print("  Resync Seqno           0x!16@XQ", FALSE, &csd->resync_seqno);
			util_out_print("  Resync trans    0x!16@XQ", TRUE, &csd->resync_tn);
		)
		UNIX_ONLY(
			util_out_print("  Zqgblmod Seqno         0x!16@XQ", FALSE, &csd->zqgblmod_seqno);
			util_out_print("  Zqgblmod Trans  0x!16@XQ", TRUE, &csd->zqgblmod_tn);
		)
		util_out_print("  Endian Format                      !6AZ", UNIX_ONLY(FALSE) VMS_ONLY(TRUE), ENDIANTHISJUSTIFY);
		UNIX_ONLY(
		util_out_print("  Commit Wait Spin Count!12UL", TRUE, csd->wcs_phase2_commit_wait_spincnt);
		)
		util_out_print("  Database file encrypted             !AD", UNIX_ONLY(FALSE) VMS_ONLY(TRUE), 5,
				  csd->is_encrypted ? " TRUE" : "FALSE");
		UNIX_ONLY(
		util_out_print("  Inst Freeze on Error         !AD", TRUE, 5, csd->freeze_on_fail ? " TRUE" : "FALSE");
		)
		UNIX_ONLY(
		util_out_print("  Spanning Node Absent                !AD", FALSE, 5, csd->span_node_absent ? " TRUE" : "FALSE");
		)
		UNIX_ONLY(
		util_out_print("  Maximum Key Size Assured     !AD", TRUE, 5, csd->maxkeysz_assured ? " TRUE" : "FALSE");
		)
	}
	if (CLI_PRESENT == cli_present("ALL"))
	{	/* Only dump if -/ALL as if part of above display */
                util_out_print(0, TRUE);
		util_out_print("                                           ", FALSE);
		util_out_print("  DB Current Minor Version      !4UL", TRUE, csd->minor_dbver);
		util_out_print("  Blks Last Record Backup        0x!XL", FALSE, csd->last_rec_bkup_last_blk);
		util_out_print("  Last GT.M Minor Version       !4UL", TRUE, csd->last_mdb_ver);
		util_out_print("  Blks Last Stream Backup        0x!XL", FALSE, csd->last_inc_bkup_last_blk);
		util_out_print("  DB Creation Version             !AD", TRUE,
			       LEN_AND_STR(gtm_dbversion_table[csd->creation_db_ver]));
		util_out_print("  Blks Last Comprehensive Backup 0x!XL", FALSE, csd->last_com_bkup_last_blk);
		util_out_print("  DB Creation Minor Version     !4UL", TRUE, csd->creation_mdb_ver);
                util_out_print(0, TRUE);
		util_out_print("  Total Global Buffers           0x!XL", FALSE, csd->n_bts);
		util_out_print("  Phase2 commit pid count 0x!XL", TRUE, cnl->wcs_phase2_commit_pidcnt);
		util_out_print("  Dirty Global Buffers           0x!XL", FALSE, cnl->wcs_active_lvl);
		util_out_print("  Write cache timer count 0x!XL", TRUE, cnl->wcs_timers);
		util_out_print("  Free  Global Buffers           0x!XL", FALSE, cnl->wc_in_free);
		util_out_print("  wcs_wtstart pid count   0x!XL", TRUE, cnl->in_wtstart);
		util_out_print("  Write Cache is Blocked              !AD", FALSE, 5, (cnl->wc_blocked ? " TRUE" : "FALSE"));
		util_out_print("  wcs_wtstart intent cnt  0x!XL", TRUE, cnl->intent_wtstart);
#		ifdef UNIX
		util_out_print(0, TRUE);
		util_out_print("  Quick database rundown is active    !AD", TRUE, 5, (csd->mumps_can_bypass ? " TRUE" : "FALSE"));
		util_out_print("  Access control rundown bypasses !9UL", FALSE, cnl->dbrndwn_access_skip);
		util_out_print("  FTOK rundown bypasses   !10UL", TRUE, cnl->dbrndwn_ftok_skip);
#		endif
		new_line = FALSE;
		for (index = 0; MAX_WTSTART_PID_SLOTS > index; index++)
		{
			pid = cnl->wtstart_pid[index];
			if (0 != pid)
			{
				util_out_print("  wcs_wtstart pid [!2UL] !AD !12UL", new_line, index,
					new_line ? 0 : 7, new_line ? "" : "       ", pid);
				new_line = !new_line;
			}
		}
		/* Additional information regarding kills that are in progress, abandoned and inhibited */
		util_out_print(0, TRUE);
		util_out_print("  Actual kills in progress     !12UL", FALSE, csd->kill_in_prog);
		util_out_print("  Abandoned Kills       !12UL", TRUE, csd->abandoned_kills);
		util_out_print("  Process(es) inhibiting KILLs        !5UL", TRUE, cnl->inhibit_kills);

		util_out_print(0, TRUE);
		util_out_print("  DB Trigger cycle of ^#t      !12UL", TRUE, csd->db_trigger_cycle);
		util_out_print(0, TRUE);
		util_out_print("  MM defer_time                       !5SL", TRUE, csd->defer_time);
		/* Print the database encryption hash information */
		GET_HASH_IN_HEX(csd->encryption_hash, outbuf, GTMCRYPT_HASH_HEX_LEN);
		util_out_print("  Database file encryption hash  !AD", TRUE, GTMCRYPT_HASH_HEX_LEN, outbuf);
	}
#	ifdef UNIX
	if (NEED_TO_DUMP("SUPPLEMENTARY"))
	{
                util_out_print(0, TRUE);
		assert(MAX_SUPPL_STRMS == ARRAYSIZE(csd->strm_reg_seqno));
		for (index = 0; index < MAX_SUPPL_STRMS; index++)
		{
			if (csd->strm_reg_seqno[index])
				util_out_print("  Stream !2UL: Reg Seqno   0x!16@XQ", TRUE, index, &csd->strm_reg_seqno[index]);
		}
	}
#	endif
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
#		define TAB_DB_CSH_ACCT_REC(COUNTER,TEXT1,TEXT2)	SHOW_DB_CSH_STAT(csd, COUNTER, TEXT1, TEXT2)
#		include "tab_db_csh_acct_rec.h"
#		undef TAB_DB_CSH_ACCT_REC
	}
	if (NEED_TO_DUMP("GVSTATS"))
	{
                util_out_print(0, TRUE);
#		define TAB_GVSTATS_REC(COUNTER,TEXT1,TEXT2)	SHOW_GVSTATS_STAT(cnl, COUNTER, TEXT1, TEXT2)
#		include "tab_gvstats_rec.h"
#		undef TAB_GVSTATS_REC
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
#		define TAB_BG_TRC_REC(A,B)	SHOW_STAT(A,B);
#		include "tab_bg_trc_rec.h"
#		undef TAB_BG_TRC_REC
	}
	jpc = csa->jnl;
	if (NEED_TO_DUMP("JOURNAL") && (JNL_ENABLED(csd) && (NULL != jpc) && (NULL != jpc->jnl_buff)))
	{
		jb = jpc->jnl_buff;
		util_out_print(0, TRUE);
		/* --------------------------- journal buffer --------------------------------- */
		util_out_print("  Jnl Buffer Size         0x!XL", FALSE, jb->size);
		util_out_print("      ", FALSE);
		util_out_print("  Dskaddr                 0x!XL", TRUE, jb->dskaddr);
		util_out_print("  Free                    0x!XL", FALSE, jb->free);
		util_out_print("      ", FALSE);
		util_out_print("  Freeaddr                0x!XL", TRUE, jb->freeaddr);
		util_out_print("  Dsk                     0x!XL", FALSE, jb->dsk);
		util_out_print("      ", FALSE);
		util_out_print("  Wrtsize                 0x!XL", TRUE, jb->wrtsize);
		util_out_print("  Journal checksum seed   0x!XL", FALSE, csd->jnl_checksum);
		util_out_print("      ", FALSE);
		util_out_print("  Min_write_size          0x!XL", TRUE, jb->min_write_size);
		util_out_print("  bytcnt                  0x!XL", FALSE, jb->bytcnt);
		util_out_print("      ", FALSE);
		util_out_print("  Max_write_size          0x!XL", TRUE, jb->max_write_size);
		util_out_print("  Before image                 !AD", FALSE, 5, (jb->before_images ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  Filesize              !12UL", TRUE, jb->filesize);
		util_out_print("  Iosb.cond             !12UW", FALSE, jb->iosb.cond);
		util_out_print("      ", FALSE);
		util_out_print("  qiocnt                !12UL", TRUE, jb->qiocnt);
		util_out_print("  Iosb.length                 0x!4XW", FALSE, jb->iosb.length);
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
		util_out_print("  Epoch_tn        0x!16@XQ", TRUE, &jb->epoch_tn);
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
		util_out_print("  fsync addrs             0x!XL", FALSE, jb->fsync_dskaddr);
		util_out_print("      ", FALSE);
		util_out_print("  Need_db_fsync                !AD", TRUE, 5, (jb->need_db_fsync ? " TRUE" : "FALSE"));
		util_out_print("  Filesystem block size   0x!XL", FALSE, jb->fs_block_size);
		util_out_print("      ", FALSE);
		util_out_print("  jnl solid tn    0x!16@XQ", TRUE, &csd->jnl_eovtn);
		for (rectype = JRT_BAD + 1; rectype < JRT_RECTYPES - 1; rectype++)
		{
			util_out_print("  Jnl Rec Type    !5AZ   0x!XL      ", FALSE, jrt_label[rectype],
				jb->reccnt[rectype]);
			rectype++;
			util_out_print("  Jnl Rec Type    !5AZ   0x!XL", TRUE, jrt_label[rectype], jb->reccnt[rectype]);
		}
		if (rectype != JRT_RECTYPES)
			util_out_print("  Jnl Rec Type    !5AZ   0x!XL", TRUE, jrt_label[rectype], jb->reccnt[rectype]);
		util_out_print(0, TRUE);
		util_out_print("  Recover interrupted          !AD", FALSE, 5, (csd->recov_interrupted ? " TRUE" : "FALSE"));
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT resolve time   !12UL", TRUE, csd->intrpt_recov_tp_resolve_time);
		util_out_print("  INTRPT jnl_state      !12UL", FALSE, csd->intrpt_recov_jnl_state);
		util_out_print("      ", FALSE);
		util_out_print("  INTRPT repl_state     !12UL", TRUE, csd->intrpt_recov_repl_state);
		util_out_print("  INTRPT seqno    0x!16@XQ", TRUE, &csd->intrpt_recov_resync_seqno);
		UNIX_ONLY(
			for (index = 0; index < MAX_SUPPL_STRMS; index++)
			{
				if (csd->intrpt_recov_resync_strm_seqno[index])
					util_out_print("  INTRPT strm_seqno :   Stream #  !2UL        Stream Seqno    0x!16@XQ",
						TRUE, index, &csd->intrpt_recov_resync_strm_seqno[index]);
			}
			for (index = 0; index < MAX_SUPPL_STRMS; index++)
			{
				if (csd->intrpt_recov_resync_strm_seqno[index])
					util_out_print("  SAVE   strm_seqno :   Stream #  !2UL        Region Seqno    0x!16@XQ",
						TRUE, index, &csd->save_strm_reg_seqno[index]);
			}
		)
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
		util_out_print("  File Offset     0x!16@XQ", TRUE, &bptr->dskaddr);
		util_out_print("  Shmpool crit holder   !12UL", FALSE, bptr->shmpool_crit_latch.u.parts.latch_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup_errno          !12UL", TRUE, bptr->backup_errno);
#		ifdef VMS
		util_out_print("  Shmpool crit imgcnt   !12UL", TRUE, bptr->shmpool_crit_latch.u.parts.latch_image_count);
#		endif
		util_out_print("  Backup Process ID     !12UL", FALSE, bptr->backup_pid);
		util_out_print("      ", FALSE);
		util_out_print("  Backup TN       0x!16@XQ", TRUE, &bptr->backup_tn);
		util_out_print("  Inc Backup TN   0x!16@XQ", FALSE, &bptr->inc_backup_tn);
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
		util_out_print("  Database WAS ONCE Fully Upgraded from V4  : !AD",
			TRUE, 5, (csd->db_got_to_v5_once ? " TRUE" : "FALSE"));
		util_out_print("  Blocks to Upgrade subzero(negative) error : 0x!XL", TRUE, csd->blks_to_upgrd_subzero_error);
		util_out_print("  TN when Blocks to Upgrade last became 0   : 0x!16@XQ", TRUE, &csd->tn_upgrd_blks_0);
		util_out_print("  TN when Desired DB Format last changed    : 0x!16@XQ", TRUE, &csd->desired_db_format_tn);
		util_out_print("  TN when REORG upgrd/dwngrd changed dbfmt  : 0x!16@XQ", TRUE, &csd->reorg_db_fmt_start_tn);
		util_out_print(0, TRUE);
		util_out_print("  Block Number REORG upgrd/dwngrd will restart from : 0x!XL",
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
#	ifdef GTM_SNAPSHOT
	if (NEED_TO_DUMP("SNAPSHOT"))
	{
		util_out_print(0, TRUE);
		util_out_print("  Snapshot in progress                 !AD", FALSE, 5,
			(cnl->snapshot_in_prog ? " TRUE" : "FALSE"));
		util_out_print("   Number of active snapshots                 !12UL", TRUE, cnl->num_snapshots_in_effect);
		util_out_print("  Snapshot cycle                 !12UL", FALSE, cnl->ss_shmcycle);
		/* SS_MULTI: Note that if we have multiple snapshots, then we have to run through each active
		 * snapshot region and dump their informations respectively
		 */
		ss_shm_ptr = (shm_snapshot_ptr_t)(SS_GETSTARTPTR(csa));
		util_out_print("  Active snapshot PID                        !12UL", TRUE, ss_shm_ptr->ss_info.ss_pid);
		util_out_print("  Snapshot TN                    !12UL", FALSE, ss_shm_ptr->ss_info.snapshot_tn);
		util_out_print("  Total blocks                               !12UL", TRUE, ss_shm_ptr->ss_info.total_blks);
		util_out_print("  Free blocks                    !12UL", FALSE, ss_shm_ptr->ss_info.free_blks);
		util_out_print("  Process failed                             !12UL", TRUE, ss_shm_ptr->failed_pid);
		util_out_print("  Failure errno                  !12UL", FALSE, ss_shm_ptr->failure_errno);
		util_out_print("  Snapshot shared memory identifier          !12SL", TRUE, ss_shm_ptr->ss_info.ss_shmid);
		util_out_print("  Snapshot file name                   !AD", TRUE, LEN_AND_STR(ss_shm_ptr->ss_info.shadow_file));
	}
#	endif
        return;
}
