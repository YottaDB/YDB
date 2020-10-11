/****************************************************************
 *								*
 * Copyright (c) 2008-2020 Fidelity National Information	*
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

/* Begin stats for GTM-8863 (please do not delete this) */
/*
 * Unfortunately we can't generate the enum for wait states directly
 * from the list below, because we need to keep the code instrumented
 * for wait states which are no longer displayed, but which may be used
 * in the future, or which are components of other states.  We go with
 * defining the first and last values
 */
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
#ifndef IS_CSD_STATS
#define WS_STATS_BEGIN f_dbext_wait
/* This first group of stats are collections of other stats (aggregates) */
TAB_GVSTATS_REC(f_dbext_wait         , "DEXA", "wait flag for ext in prog             ")
TAB_GVSTATS_REC(f_bg_wait            , "GLB",  "wait flag for bg acc in prog          ")
TAB_GVSTATS_REC(f_jnl_wait           , "JNL",  "wait flag for jnl acc in prog         ")
TAB_GVSTATS_REC(f_mlk_wait           , "MLK",  "wait flag for mlk acc in prog         ")
TAB_GVSTATS_REC(f_proc_wait          , "PRC",  "wait flag for proc cleanup in prog    ")
TAB_GVSTATS_REC(f_trans_wait         , "TRX",  "wait flag for trans in prog           ")
TAB_GVSTATS_REC(f_util_wait          , "ZAD",  "wait flag for utility cmd in prog     ")

TAB_GVSTATS_REC(f_ws2                , "JOPA", "wait flag for journal open in prog    ")
TAB_GVSTATS_REC(f_ws12               , "AFRA", "wait flag for auto freeze release     ")
TAB_GVSTATS_REC(f_ws15               , "BREA", "wait flag for blk rd encryp cycle sync")
TAB_GVSTATS_REC(f_ws39               , "MLBA", "wait flag for mlk acquire blocked     ")
TAB_GVSTATS_REC(f_ws47               , "TRGA", "wait flag for grab region for trans   ")
#define WS_STATS_END f_ws47
/* End stats for GTM-8863 (please do not delete this) */
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
 */
#endif
