/****************************************************************
 *								*
 * Copyright (c) 2008-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note that the order these fields appear in below is the same order the fields are defined in the
 * gvstats_rec_t structure (fileheader, sgmnt_addrs, and node_local). They also represent the order
 * these fields are displayed by zshow_gvstats() (ZSHOW "G"). Also the order the stats present themselves
 * is assumed to be the order of the binary stats by ^%YGBLSTAT.m when processing shared global stats.
 * Therefore, any operation that changes the order or offset of any given field should not be attempted.
 * Additions are to be done at the END of the file.
 *
 * Replacing existing fields with new fields is allowed (provided their implications are thoroughly analyzed).
 */
TAB_GVSTATS_REC(n_set                , "SET", "# of SET   operations                 ")
TAB_GVSTATS_REC(n_kill               , "KIL", "# of KILl  operations                 ")
TAB_GVSTATS_REC(n_get                , "GET", "# of GET   operations                 ")
TAB_GVSTATS_REC(n_data               , "DTA", "# of DaTA  operations                 ")
TAB_GVSTATS_REC(n_order              , "ORD", "# of ORDer operations                 ")
TAB_GVSTATS_REC(n_zprev              , "ZPR", "# of ZPRevious operations             ")
TAB_GVSTATS_REC(n_query              , "QRY", "# of QueRY operations                 ")
TAB_GVSTATS_REC(n_lock_success       , "LKS", "# of LocK calls that Succeeded        ")
TAB_GVSTATS_REC(n_lock_fail          , "LKF", "# of LocK calls that Failed           ")
TAB_GVSTATS_REC(db_curr_tn           , "CTN", "Current Transaction Number of db      ")
TAB_GVSTATS_REC(n_dsk_read           , "DRD", "# of Disk ReaDs from database file    ")
TAB_GVSTATS_REC(n_dsk_write          , "DWT", "# of Disk WriTes to database file     ")
TAB_GVSTATS_REC(n_nontp_readwrite    , "NTW", "# of Non-tp Tns that were read-Write  ")
TAB_GVSTATS_REC(n_nontp_readonly     , "NTR", "# of Non-tp Tns that were Read-only   ")
TAB_GVSTATS_REC(n_nontp_blkwrite     , "NBW", "# of Non-tp induced Block Writes      ")
TAB_GVSTATS_REC(n_nontp_blkread      , "NBR", "# of Non-tp induced Block Reads       ")
TAB_GVSTATS_REC(n_nontp_retries_0    , "NR0", "# of Non-tp Retries [0]               ")
TAB_GVSTATS_REC(n_nontp_retries_1    , "NR1", "# of Non-tp Retries [1]               ")
TAB_GVSTATS_REC(n_nontp_retries_2    , "NR2", "# of Non-tp Retries [2]               ")
TAB_GVSTATS_REC(n_nontp_retries_3    , "NR3", "# of Non-tp Retries [3]               ")
TAB_GVSTATS_REC(n_tp_readwrite       , "TTW", "# of Tp Tns that were read-Write      ")
TAB_GVSTATS_REC(n_tp_readonly        , "TTR", "# of Tp Tns that were Read-only       ")
TAB_GVSTATS_REC(n_tp_rolledback      , "TRB", "# of Tp tns that were Rolled Back     ")
TAB_GVSTATS_REC(n_tp_blkwrite        , "TBW", "# of Tp induced Block Writes          ")
TAB_GVSTATS_REC(n_tp_blkread         , "TBR", "# of Tp induced Block Reads           ")
TAB_GVSTATS_REC(n_tp_tot_retries_0   , "TR0", "# of Tp total Retries [0]             ")
TAB_GVSTATS_REC(n_tp_tot_retries_1   , "TR1", "# of Tp total Retries [1]             ")
TAB_GVSTATS_REC(n_tp_tot_retries_2   , "TR2", "# of Tp total Retries [2]             ")
TAB_GVSTATS_REC(n_tp_tot_retries_3   , "TR3", "# of Tp total Retries [3]             ")
TAB_GVSTATS_REC(n_tp_tot_retries_4   , "TR4", "# of Tp total Retries [4]             ")
TAB_GVSTATS_REC(n_tp_cnflct_retries_0, "TC0", "# of Tp Conflict retries [0]          ")
TAB_GVSTATS_REC(n_tp_cnflct_retries_1, "TC1", "# of Tp Conflict retries [1]          ")
TAB_GVSTATS_REC(n_tp_cnflct_retries_2, "TC2", "# of Tp Conflict retries [2]          ")
TAB_GVSTATS_REC(n_tp_cnflct_retries_3, "TC3", "# of Tp Conflict retries [3]          ")
TAB_GVSTATS_REC(n_tp_cnflct_retries_4, "TC4", "# of Tp Conflict retries [4]          ")
TAB_GVSTATS_REC(n_ztrigger           , "ZTR", "# of ZTRigger operations              ")
TAB_GVSTATS_REC(n_db_flush           , "DFL", "# of Database FLushes                 ")
TAB_GVSTATS_REC(n_db_fsync           , "DFS", "# of Database FSyncs                  ")
TAB_GVSTATS_REC(n_jnl_flush          , "JFL", "# of Journal FLushes                  ")
TAB_GVSTATS_REC(n_jnl_fsync          , "JFS", "# of Journal FSyncs                   ")
TAB_GVSTATS_REC(n_jbuff_bytes        , "JBB", "# of Bytes written to Journal Buffer  ")
TAB_GVSTATS_REC(n_jfile_bytes        , "JFB", "# of Bytes written to Journal File    ")
TAB_GVSTATS_REC(n_jfile_writes       , "JFW", "# of Journal File Writes              ")
TAB_GVSTATS_REC(n_jrec_logical       , "JRL", "# of Logical Journal Records          ")
TAB_GVSTATS_REC(n_jrec_pblk          , "JRP", "# of Pblk Journal Records             ")
TAB_GVSTATS_REC(n_jrec_epoch_regular , "JRE", "# of Regular Epoch Journal Records    ")
TAB_GVSTATS_REC(n_jrec_epoch_idle    , "JRI", "# of Idle epoch Journal Records       ")
TAB_GVSTATS_REC(n_jrec_other         , "JRO", "# of Other Journal Records            ")
TAB_GVSTATS_REC(n_jnl_extends        , "JEX", "# of Journal file EXtensions          ")
TAB_GVSTATS_REC(n_db_extends         , "DEX", "# of Database file EXtensions         ")
TAB_GVSTATS_REC(n_crit_success       , "CAT", "# of crit acquired total successes    ")
TAB_GVSTATS_REC(n_crits_in_epch      , "CFE", "# of attempts in CFT caused by epochs ")
TAB_GVSTATS_REC(sq_crit_failed       , "CFS", "sum squares grab crit failed          ")
TAB_GVSTATS_REC(n_crit_failed        , "CFT", "# of grab crit failures               ")
TAB_GVSTATS_REC(sq_crit_que_slps     , "CQS", "sum squares grab crit queued sleeps   ")
TAB_GVSTATS_REC(n_crit_que_slps      , "CQT", "# of grab crit queued sleeps          ")
TAB_GVSTATS_REC(sq_crit_yields       , "CYS", "sum squares grab crit yields          ")
TAB_GVSTATS_REC(n_crit_yields        , "CYT", "# of grab crit yields                 ")
TAB_GVSTATS_REC(n_clean2dirty        , "BTD", "# of Block Transitions to Dirty       ")
TAB_GVSTATS_REC(n_wait_for_read      , "WFR", "# of Blocks needing sleep on the read ") /* was t_qread_ripsleep_cnt in BG_TRC_REC */
TAB_GVSTATS_REC(n_buffer_scarce      , "BUS", "# times db_csh_get too many retries   ") /*was db_csh_get_too_many_loops BG_TRC_REC*/
TAB_GVSTATS_REC(n_bt_scarce          , "BTS", "# times db_csh_get too many retries   ") /* was bt_put_flush_dirty in BG_TRC_REC */
TAB_GVSTATS_REC(n_set_trigger_fired  , "STG", "# of SET triggers fired               ")
TAB_GVSTATS_REC(n_kill_trigger_fired , "KTG", "# of KILL triggers fired              ")
TAB_GVSTATS_REC(n_ztrigger_fired     , "ZTG", "# of ZTRIGGERs fired                  ")

#define FIRST_WS_STATE 1
#define LAST_WS_STATE 102

/* The gvstats record exists in two different versions, which may or may not
 * be identical: gvstats_rec_csd_t & gvstats_rec_t.  They *are* identical
 * up to this point in the statistics definitions.  After this point, if
 * IS_CSD_STATS is not defined, the remaining statistics will *not* be
 * present in gvstats_rec_csd_t.  This is the record which is stored in the
 * database file header.  The effect is that stats in gvstats_rec_t, but not
 * gvstats_rec_csd_t will be displayed, and can be shared and queried by ^%YGBLSTAT
 * but will not be stored after a GT.M process exits.  The rationale for this is
 * twofold: 1) It decreases what is stored in the fileheader, avoiding the need to
 * allocate more space there & 2) the GTM-8863 statistics are snapshots into a
 * running process, and not cumulative values it makes sense to sum & store.
 *
 * As it turns out, the number of statistics created by GTM-8863 has been winnowed
 * to the point that they could be stored without a file header expansion.  To do
 * this, comment out the #ifndef below, or edit gvstats_rec.h
 *
 * The current status is that all stats are in both records.  This avoids the
 * bad user experience of 'write $VIEW("GVSTAT","DEFAULT")' showing something
 * different than "mumps -run %YGBLSTAT".  The trade-off is special code in gvstats_rec.c
 * to avoid storing the stats below (though the header space is still allocated).
 */
/* This first group of stats are collections of other stats (aggregates) */
TAB_GVSTATS_REC(n_dbext_wait         , "DEXA", "counter for ext in prog               ")
TAB_GVSTATS_REC(n_bg_wait            , "GLB",  "counter for bg acc in prog            ")
TAB_GVSTATS_REC(n_jnl_wait           , "JNL",  "counter for jnl acc in prog           ")
TAB_GVSTATS_REC(n_mlk_wait           , "MLK",  "counter for mlk acc in prog           ")
TAB_GVSTATS_REC(n_proc_wait          , "PRC",  "counter for proc cleanup in prog      ")
TAB_GVSTATS_REC(n_trans_wait         , "TRX",  "counter for trans in prog             ")
/* Cachline border (128-byte and 64-byte) */
TAB_GVSTATS_REC(n_util_wait          , "ZAD",  "counter for utility cmd in prog       ")
TAB_GVSTATS_REC(n_ws2                , "JOPA", "counter for journal open in prog      ")
TAB_GVSTATS_REC(n_ws12               , "AFRA", "counter for auto freeze release       ")
TAB_GVSTATS_REC(n_ws15               , "BREA", "counter for blk rd encryp cycle sync  ")
TAB_GVSTATS_REC(n_ws39               , "MLBA", "counter for mlk acquire blocked       ")
TAB_GVSTATS_REC(n_ws47               , "TRGA", "counter for grab region for trans     ")
/* Read/Write helper counters */
TAB_GVSTATS_REC(n_wait_read_long     , "WRL",  "# times sleep read exceeds counter X  ")
TAB_GVSTATS_REC(n_pre_read_globals   , "PRG",  "# of pre-read globals                 ")
/* Cacheline border (64-byte) */
TAB_GVSTATS_REC(n_writer_flush       , "WFL",  "# of DB FLushes by the writer helpers ")
TAB_GVSTATS_REC(n_writer_helper_epoch, "WHE",  "# of waits for jnl write lock or fsync")
/* This aggregate is also included in n_set */
/* New sleep statistics. If possible, in-crit counters should not share cacheline with out-of-crit counters
 * Otherwise, the least active crit counters should share the out-of-crit cacheline. */
TAB_GVSTATS_REC(ms_jnl_critsleeps    , "JCS",  "ms of sleep time for jnl write in crit")
TAB_GVSTATS_REC(ms_jnl_nocritsleeps  , "JNS",  "ms of sleep time for jnl write no crit")
TAB_GVSTATS_REC(ms_wip_critsleeps    , "WPCS", "ms of sleep time for wip in crit      ")
TAB_GVSTATS_REC(ms_rip_critsleeps    , "RCS",  "ms of sleep time for rip in crit      ")
TAB_GVSTATS_REC(ms_flu_critsleeps    , "WFCS", "ms of sleep time for wcs_flu in crit  ")
TAB_GVSTATS_REC(ms_getn_critsleeps   , "GCS",  "ms of sleep time for csh_getn in crit ")
/* Cacheline border (128-byte and 64-byte) */
TAB_GVSTATS_REC(n_increment          , "INC",  "# of INCREMENT operations             ")
TAB_GVSTATS_REC(n_cache_reads	     , "CRD",  "# of cache reads                      ")
/* If new stats are added beyond this point, edit gvstats_rec_cnl2csd in gvstats_rec.c */

/*
 * At some point the stats below were decompsed into more granular stats.  Looking back
 * through git, we can reverse this decomposition, to reconstruct the stats below, which
 * are still of interest.
 *
 * Begin Aggregate Stats Definition (please do not delete this)
 *
 * DEXA: WS_11 WS_12
 * GLB: WS_23 WS_24 WS_25 WS_26 WS_27 WS_13 WS_14 WS_15 WS_16 WS_17 WS_18 WS_19 WS_20 WS_21 WS_22
 * JNL: WS_2 WS_3 WS_28 WS_4 WS_29 WS_30 WS_31 WS_32 WS_33 WS_34 WS_35 WS_36 WS_37
 * MLK: WS_5 WS_38 WS_39
 * PRC: WS_40 WS_41
 * TRX: WS_43 WS_44 WS_45 WS_46 WS_47 WS_51 WS_52 WS_53 WS_54 WS_48 WS_49 WS_50
 * ZAD: WS_1 WS_10 WS_100 WS_101 WS_102 WS_55 WS_56 WS_57 WS_58 WS_59 WS_6 WS_60 WS_61
 * ZAD: WS_62 WS_63 WS_64 WS_65 WS_66 WS_67 WS_68 WS_69 WS_7 WS_70 WS_71 WS_72
 * ZAD: WS_73 WS_74 WS_75 WS_76 WS_77 WS_78 WS_79 WS_8 WS_80 WS_81 WS_82 WS_83
 * ZAD: WS_84 WS_85 WS_86 WS_87 WS_88 WS_89 WS_9 WS_90 WS_91 WS_92 WS_93 WS_94
 * ZAD: WS_95 WS_96 WS_97 WS_98 WS_99
 *
 * End Aggregate Stats Definition (please do not delete this)
 *
 * These are brief descriptions of why crit is grabbed for a particular WS
 * In general, this command should suss the WS instances out of the code:
 *	grep -E 'WS_[0-9]+\)|WS_[0-9]+,' $work_dir/gtm/{sr_port,sr_unix}/ * | grep -v proc_wait_stat.h
 *                                                                     ^(note no space before star when typed)
 *
 * Begin Non-Exposed Wait State Definitions (please do not delete this)
 *	WS_1: Change database version
 *	WS_2: Ensure journal file is open
 *	WS_3: Grab latest journal for flush if needed
 *	WS_4: Update journal for region
 *	WS_5: Lock when lock shares DB critical section
 *	WS_6: Copy region file header for integ
 *	WS_7: Process IO waiting for read-only DB during mupip integ
 *	WS_8: Mupip upgrade/downgrade
 *	WS_9: Mupip upgrade/downgrade interrupted
 *	WS_10: Mupip online backup
 *	WS_11: Remap db after file ext in MM access mode
 *	WS_12: File extension in progress
 *	WS_13: Global variable kill expand free subtree
 *	WS_14: Update decryption keys if needed before block read
 *	WS_15: Update decryption keys if needed for read of block not in cache
 *	WS_16: Recover from commit timeout during block read
 *	WS_17: Recover from buffer blocked on pid during block read
 *	WS_18: Final try for block read
 *	WS_19: Update process database critical section to reset history stream related information
 *	WS_20: Buffer flush access to queue of free buffers
 *	WS_21: Probe crit access to waiting process queue
 *	WS_22: Database critical section to assess BG
 *	WS_23: Lock access to ASYNC IO Write In Progress (WIP) queue
 *	WS_24: Recover from phase 2 commit fail
 *	WS_25: Remove cr from wip queue.
 *	WS_26: Flush cr from active queue.
 *	WS_27: Reap i/o requests waiting for free buffers
 *	WS_28: Phase 2 commit cleanup
 *	WS_29: Check journal buffer during phase 2 commit
 *	WS_30: $View() debug facility grab journal phase 2 commit latch
 *	WS_31: $View() debug facility grab journal pool phase 2 commit latch
 *	WS_32: View command JNLFLUSH facility
 *	WS_33: Journal cleanup grab journal phase 2 commit latch
 *	WS_34: Sync replicated database file header sequence number
 *	WS_35: Grab journal buffer pool I/O in progress latch for mutex salvage
 *	WS_36: Grab journal buffer pool fsync latch for mutex salvage
 *	WS_37: Set replicated database file header values "zqgblmod_seqno" & "zqgblmod_tn" to 0
 *	WS_38: Grab critical latch for lock operations
 *	WS_39: Reattempt blocked lock
 *	WS_40: Mupip journal recover orphan block
 *	WS_41: Close journal file cleanly
 *	WS_42: DB rundown flush fileheader to disk
 *	WS_43: Transaction begin code for write-after images (DSE)
 *	WS_44: Cleanup non-TP commit
 *	WS_45: Ensure non-TP read transaction consistency
 *	WS_46: Check for wrong root block in non-TP read transaction
 *	WS_47: Hold up on hard freeze ahead of non-TP trans validation
 *	WS_48: Resume crit after a reorg encrypt during transaction retry
 *	WS_49: Sync encryption cycles during transaction retry
 *	WS_50: Wait for unfrozen db on final transaction retry
 *	WS_51: Sync online rollback cycles during TP transaction history validation
 *	WS_52: Prepare for wcs_recover() in TP restart
 *	WS_53: Grab crit for all regions of a TP transaction when needed prior to TCOMMIT
 *	WS_54: Grab crit on all TP regions while validating transaction for commit
 *	WS_55: Grab crit for DSE functions
 *	WS_56: DSE 'seize crit' operation
 *	WS_57: DSE 'renew' or 'wcinit' operation
 *	WS_58: DSE grab critical section for write (unused?)
 *	WS_59: DSE grab critical section for '-restore all'
 *	WS_60: DSE grab critical section for '-master'
 *	WS_61: DSE grab critical section to pin down free block information
 *	WS_62: DSE grab critical section for database region cache reinitialize
 *	WS_63: DSE grab freeze latch for chilled autorelease
 *	WS_64: Read block for mupip upgrade/downgrade
 *	WS_65: Write same journal timestamp for all regions
 *	WS_66: Adjust global journal record time after KIP wait
 *	WS_67: Lock region for mupip extend
 *	WS_68: Ensure region unfrozen for mupip extend
 *	WS_69: Lock region for mupip reorg truncate
 *	WS_70: Mark mupip reorg truncate failure for region
 *	WS_71: Lock region for mupip set journal
 *	WS_72: Harden journal files to disk before recover/rollback
 *	WS_73: Grab regions for online rollback
 *	WS_74: Adjust transaction number after killed process
 *	WS_75: Handle PFIN in mupip recover
 *	WS_76: Explicitly invoked by 'GRABCRIT' operand for 'view'
 *	WS_77: In 'PROBECRIT' operand for 'view'
 *	WS_78: For WB test in 'view "STORDUMP"' grab/hold crit up to MUTEXLCKALERT_INTERVAL * 12
 *	WS_79: Grab crit for region freeze if not already held
 *	WS_80: Resume crit after kill in progress during region freeze
 *	WS_81: Grab freeze latch during region freeze
 *	WS_82: Grab freeze latch during region unfreeze
 *	WS_83: Region journal file switch after region freeze
 *	WS_84: Switch journal file after updating history
 *	WS_85: Correct extract count after failed mupip extract
 *	WS_86: Update region extract count for mupip extract
 *	WS_87: Free journal control structure during db file rundown
 *	WS_88: Start mupip truncate phase 2
 *	WS_89: Allow existing phase 2 commits to complete during mupip backup
 *	WS_90: Allow existing phase 2 commits to complete during mupip incremental backup
 *	WS_91: Wait for concurrent i/o, update encr cyc, sync header in mupip reorg encrypt
 *	WS_92: Note USED blocks for encryption in mupip reorg encrypt
 *	WS_93: Mupip reorg encrypt wait for I/O completion, do post encr housekeeping
 *	WS_94: Reseize crit on mupip reorg encrypt error, do final housekeeping
 *	WS_95: Protect mupip set file operations when 'standalone' is not required
 *	WS_96: Decrement the inhibit_kills counter safely
 *	WS_97: Do shared memory housekeeping before starting snapshot
 *	WS_98: Check if another process has added blocks before starting snapshot
 *	WS_99: Wait for pending phase 2 updates to finish during snapshot
 *	WS_100: Switch journal file as mupip trigger upgrade commits
 *	WS_101: Protect ^#t upgrade flag during set if not already in crit
 *	WS_102: Protect housekeeping variables during multiproc online freeze
 * End Non-Exposed Wait State Definitions (please do not delete this)
 */
