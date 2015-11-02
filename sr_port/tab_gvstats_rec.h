/****************************************************************
 *								*
 *	Copyright 2008, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Note that each TAB_GVSTATS_REC entry corresponds to a field in the file-header
 * Therefore, any operation that changes the offset of any entry in the file-header shouldn't be attempted.
 * Additions are to be done at the END of the file.
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
