/****************************************************************
 *								*
 * Copyright (c) 2005-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef WBOX_TEST_INIT
#define WBOX_TEST_INIT

/* The standalone routine gtmsecshr_wrapper needs these to be GBLDEFs so create a scheme to force that. If WBOX_GBLDEF is
 * defined, create these as GBLDEFs instead of GBLREFs.
 */
#ifdef WBOX_GBLDEF
# define REFTYPE GBLDEF
#else
# define REFTYPE GBLREF
#endif
#include <stdlib.h> /* For setenv used below */
REFTYPE	boolean_t	ydb_white_box_test_case_enabled;
REFTYPE	int		ydb_white_box_test_case_number;
REFTYPE	int		ydb_white_box_test_case_count;
REFTYPE	int 		gtm_wbox_input_test_case_count;

void wbox_test_init(void);

/* List of whitebox testcases */
typedef enum {
	WBTEST_T_END_JNLFILOPN = 1,		/*  1 */
	WBTEST_TP_TEND_JNLFILOPN,		/*  2 */
	WBTEST_TP_TEND_TRANS2BIG,		/*  3 */
	WBTEST_BG_UPDATE_BTPUTNULL,		/*  4 */
	WBTEST_BG_UPDATE_DBCSHGET_INVALID,	/*  5 */
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID,	/*  6 */
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID2,	/*  7 : twin logic */
	WBTEST_BG_UPDATE_READINPROGSTUCK1,	/*  8 */
	WBTEST_BG_UPDATE_READINPROGSTUCK2,	/*  9 */
	WBTEST_BG_UPDATE_DIRTYSTUCK1,		/* 10 : dirty wait logic */
	WBTEST_BG_UPDATE_DIRTYSTUCK2,		/* 11 */
	WBTEST_BG_UPDATE_INTENDSTUCK,		/* 12 */
	WBTEST_BG_UPDATE_INSQTIFAIL,		/* 13 */
	WBTEST_BG_UPDATE_INSQHIFAIL,		/* 14 */
	WBTEST_BG_UPDATE_PHASE2FAIL,		/* 15 */
	WBTEST_JNL_FILE_LOST_DSKADDR,		/* 16 */
	WBTEST_REPL_HEARTBEAT_NO_ACK,		/* 17 */
	WBTEST_REPL_TEST_UNCMP_ERROR,		/* 18 */
	WBTEST_REPL_TR_UNCMP_ERROR,		/* 19 */
	WBTEST_TP_HIST_CDB_SC_BLKMOD,		/* 20 */
	WBTEST_ABANDONEDKILL,			/* 21 : MUPIP STOP a kill in progress in 2nd stage*/
	WBTEST_ENCRYPT_INIT_ERROR,		/* 22 : Prevent encryption-initialized assert from happening */
	WBTEST_UPD_PROCESS_ERROR,		/* 23 : Update process should issue GVSUBOFLOW error, REC2BIG error */
	WBTEST_FILE_EXTEND_ERROR,		/* 24 : Prevent assert from mupip extend if # blocks is > MAXTOTALBLKS
						 *	(see gdsfhead.h for macro definition) */
	WBTEST_BUFOWNERSTUCK_STACK,		/* 25 : Get stack trace of the blocking pid for stuck messages*/
	WBTEST_OINTEG_WAIT_ON_START,		/* 26 : Have online integ wait 60 after initiating the snapshot */
	WBTEST_MUR_ABNORMAL_EXIT_EXPECTED,	/* 27 : We expect MUPIP JOURNAL RECOVER/ROLLBACK to exit with non-zero status */
	WBTEST_REPLBRKNTRANS,			/* 28 : We expect a REPLBRKNTRANS error from the source server */
	WBTEST_CRASH_SHUTDOWN_EXPECTED,		/* 29 : Prevent assert if a crash shutdown is attempted (by kill -9) that
						 *	could potentially leave some dirty buffers NOT to be flushed to the disk */
	WBTEST_HELPOUT_TRIGNAMEUNIQ,		/* 30 : Use to skip alphanumeric processing in gtm_trigger_complink so as to
						 *	test TRIGNAMEUNIQ error */
	WBTEST_HELPOUT_TRIGDEFBAD,		/* 31 : Use to prevent asserts from tripping while intentionally testing ^#t
						 *	integrity (in gv_trigger.c) */
	WBTEST_SEMTOOLONG_STACK_TRACE,		/* 32 : Get stack trace of the process which is holding on to
						 *	the semaphore too long */
	WBTEST_INVALID_SNAPSHOT_EXPECTED,	/* 33 : Prevent asserts from tripping in case of an invalid snapshot */
	WBTEST_JNLOPNERR_EXPECTED,		/* 34 : Prevent asserts in jnl_file_open in case of a JNLMOVED error */
	WBTEST_FILE_EXTEND_INTERRUPT_1,		/* 35 : Unix only.  Freeze before 1st fsync */
	WBTEST_FILE_EXTEND_INTERRUPT_2,		/* 36 : Unix only.  Freeze after 1st fsync */
	WBTEST_FILE_EXTEND_INTERRUPT_3,		/* 37 : Unix only.  Freeze before 2nd fsync */
	WBTEST_FILE_EXTEND_INTERRUPT_4,		/* 38 : Unix only.  Freeze after 2nd fsync */
	WBTEST_FILE_EXTEND_INTERRUPT_5,		/* 39 : Unix only.  Freeze before file header fsync */
	WBTEST_FILE_EXTEND_INTERRUPT_6,		/* 40 : Unix only.  Freeze after file header fsync */
	WBTEST_JNL_CREATE_INTERRUPT,		/* 41 : Freeze before closing new journal file */
	WBTEST_JNL_CREATE_FAIL,			/* 42 : Journal file creation always return EXIT_ERR */
	WBTEST_JNL_FILE_OPEN_FAIL,		/* 43 : Unix only.  Journal file open always return ERR_JNLFILOPN */
	WBTEST_FAIL_ON_SHMGET,			/* 44 : Unix only.  Cause db_init() to fail on shmget */
	WBTEST_EXTEND_JNL_FSYNC,		/* 45 : enter a long loop upon trying to do jnl_fsync */
	WBTEST_TRIGR_TPRESTART_MSTOP,		/* 46 : Trigger being restarted gets a MUPIP STOP - shouldn't fail */
	WBTEST_SENDTO_EPERM,			/* 47 : Will sleep in grab_crit depending on ydb_white_box_test_case_number */
	WBTEST_ALLOW_ARBITRARY_FULLY_UPGRADED,	/* 48 : Allows csd->fully_upgraded to take arbitrary values (via DSE) and prevents
						 *      assert in mur_process_intrpt_recov.c */
	WBTEST_HOLD_ONTO_FTOKSEM_IN_DBINIT,	/* 49 : Sleep in db_init after getting hold of the ftok semaphore */
	WBTEST_HOLD_ONTO_ACCSEM_IN_DBINIT,	/* 50 : Sleep in db_init after getting hold of the access control semaphore */
	WBTEST_UNUSED51,			/* 51 : UNUSED - YDB#235 removed the only place to test this */
	WBTEST_SYSCONF_WRAPPER,			/* 52 : Will sleep in SYSCONF wrapper to let us verify that first two MUPIP STOPs
						 *	are indeed deferred in the interrupt-deferred zone, but the third isn't */
	WBTEST_DEFERRED_TIMERS,			/* 53 : Will enter a long loop upon specific WRITE or MUPIP STOP command */
	WBTEST_BREAKMPC,			/* 54 : Breaks the mpc of the previous frame putting 0xdeadbeef in it */
	WBTEST_CRASH_TRUNCATE_1,		/* 55 : Issue a kill -9 before 1st fsync */
	WBTEST_CRASH_TRUNCATE_2,		/* 56 : Issue a kill -9 after 1st fsync */
	WBTEST_CRASH_TRUNCATE_3,		/* 57 : Issue a kill -9 after reducing csa->ti->total_blks, before FTRUNCATE */
	WBTEST_CRASH_TRUNCATE_4,		/* 58 : Issue a kill -9 after FTRUNCATE, before 2nd fsync */
	WBTEST_CRASH_TRUNCATE_5,		/* 59 : Issue a kill -9 after after 2nd fsync */
	WBTEST_HOLD_SEM_BYPASS,			/* 60 : Hold access and FTOK semaphores so that LKE/DSE can bypass it. */
	WBTEST_UTIL_OUT_BUFFER_PROTECTION,	/* 61 : Start a timer that would mess with util_out buffers by frequently
						 *	printing long messages via util_out_print */
	WBTEST_WSSTATS_PAUSE,			/* 62 : Pause in GTM-8863 stat testing for sync (Reused WBTEST_SET_WC_BLOCKED)*/
	WBTEST_REORG_DEBUG,			/* 63 : mupip reorg Will print GTMPOOLLIMIT value */
	WBTEST_WCS_FLU_IOERR,			/* 64 : Force an I/O error (other than ENOSPC) when wcs_wtstart is invoked from
						 *      wcs_flu */
	WBTEST_WCS_WTSTART_IOERR,		/* 65 : Force an I/O error (other than ENOSPC) within wcs_wtstart */
	WBTEST_HOLD_CRIT_TILL_LCKALERT,		/* 66 : Grab and hold crit until 15 seconds past what triggers a lock alert message
						 *      which should invoke a mutex salvage */
	WBTEST_OPER_LOG_MSG,			/* 67 : send message to operator log */
	WBTEST_FAKE_BIG_CNTS	,		/* 68 : fake large increase in database counts to show they don't overflow  */
	/* Begin ANTIFREEZE related white box test cases */
	WBTEST_ANTIFREEZE_JNLCLOSE,		/* 69 :  */
	WBTEST_ANTIFREEZE_DBBMLCORRUPT,		/* 70 :  */
	WBTEST_CUSTERR_FREEZE,			/* 71 :  Force an undocumented error and see if it freezes an instance */
	WBTEST_ANTIFREEZE_DBFSYNCERR,		/* 72 :	 */
	WBTEST_ANTIFREEZE_GVDATAFAIL,		/* 73 :  */
	WBTEST_ANTIFREEZE_GVGETFAIL,		/* 74 :  */
	WBTEST_ANTIFREEZE_GVINCRPUTFAIL,	/* 75 :  */
	WBTEST_ANTIFREEZE_GVKILLFAIL,		/* 76 :  */
	WBTEST_ANTIFREEZE_GVORDERFAIL,		/* 77 :  */
	WBTEST_ANTIFREEZE_GVQUERYFAIL,		/* 78 :  */
	WBTEST_ANTIFREEZE_GVQUERYGETFAIL,	/* 79 :  */
	WBTEST_BACKUP_FORCE_SLEEP,		/* 80 :  Force a BACKUP -DATABSE to sleep just before or during the copy phase */
	WBTEST_ANTIFREEZE_OUTOFSPACE,		/* 81 :  */
	/* End ANTIFREEZE related white box test cases */
	WBTEST_SIGTSTP_IN_JNL_OUTPUT_SP,	/* 82 : Send SIGTSTP to self if wcs_timers is 0 */
	WBTEST_FORCE_SEMGETERROR,		/* 83 : Send SEMGETERROR to gtmsecshr to validate it handles it correctly */
	WBTEST_LONGSLEEP_IN_REPL_SHUTDOWN,	/* 84 : Sleep in Source/Receiver shutdown logic to ensure sem/shm is not removed */
	WBTEST_FORCE_WCS_GET_SPACE,		/* 85 : Simulate state in which nearly all global buffers are dirty, forcing
						 *      wcs_get_space to be called before committing an update */
	/* Begin HugeTLB tests */
	WBTEST_HUGETLB_DLOPEN,			/* 86 : Fail dlopen(libhugetlbfs.so) */
	WBTEST_HUGETLB_DLSYM,			/* 87 : Fail dlsym(shmget) */
	WBTEST_FSYNC_SYSCALL_FAIL,		/* 88 : Force error from fsync() */
	WBTEST_HUGE_ALLOC,			/* 89 : Force ZALLOCSTOR, ZREALSTOR, and ZUSEDSTOR to be values exceeding
						 *	the capacity of four-byte ints */
	WBTEST_MEM_MAP_SYSCALL_FAIL,		/* 90 : Force shmat() on AIX and mmap() on other platforms to return an error */
	WBTEST_TAMPER_HOSTNAME,			/* 91 : Change host name in db_init to call condition handler */
	WBTEST_RECOVER_ENOSPC,			/* 92 : Cause ENOSPC error on Xth write to test return status on error */
	WBTEST_WCS_FLU_FAIL,			/* 93 : Simulates a failure in wcs_flu */
	WBTEST_PREAD_SYSCALL_FAIL,		/* 94 : Simulate pread() error in dsk_read */
	WBTEST_HOLD_CRIT_ENABLED,		/* 95 : Enable $view("PROBECRIT","REGION") command to cold crit */
	WBTEST_HOLD_FTOK_UNTIL_BYPASS,		/* 96 : Hold the ftok semaphore until another process comes and bypasses it */
	WBTEST_SLEEP_IN_WCS_WTSTART,		/* 97 : Sleep in one of the predetermined places inside wcs_wtstart.c */
	WBTEST_SETITIMER_ERROR,			/* 98 : Simulate an error return from setitimer in gt_timers.c */
	WBTEST_HOLD_GTMSOURCE_SRV_LATCH,	/* 99 : Hold the source server latch until rollback process issues a SIGCONT */
	WBTEST_KILL_ROLLBACK,			/* 100 : Kill in the middle of rollback */
	WBTEST_INFO_HUB_SEND_ZMESS,		/* 101 : Print messages triggered via ZMESSAGE to the syslog */
	WBTEST_SKIP_CORE_FOR_MEMORY_ERROR,	/* 102 : Do not generate core file in case of YDB-E-MEMORY fatal error */
	WBTEST_EXTFILTER_INDUCE_ERROR,		/* 103 : Do not assert in case of external filter error (test induces that) */
	WBTEST_BADEXEC_UPDATE_PROCESS,		/* 104 : Prevent the update process from EXECing */
	WBTEST_BADEXEC_HELPER_PROCESS,		/* 105 : Prevent the helper processes from EXECing */
	WBTEST_BADEDITOR_GETEDITOR,		/* 106 : Have geteditor return that it was unable to find an editor */
	WBTEST_BADEXEC_OP_ZEDIT,		/* 107 : Give a invalid executable path to an editor so EXEC will fail */
	WBTEST_BADEXEC_SECSHR_PROCESS,		/* 108 : Prevent the SECSHR process from being EXEC'ed */
	WBTEST_BADDUP_PIPE_STDIN,		/* 109 : Prevent dup2() of stdin in forked piped process */
	WBTEST_BADDUP_PIPE_STDOUT,		/* 110 : Prevent dup2() of stdout in forked piped process */
	WBTEST_BADDUP_PIPE_STDERR1,		/* 111 : Prevent dup2() of stderr in forked piped process */
	WBTEST_BADDUP_PIPE_STDERR2,		/* 112 : Prevent second dup2() of stderr in forked piped process */
	WBTEST_BADEXEC_PIPE_PROCESS,		/* 113 : Prevent the SECSHR process from being EXEC'ed */
	WBTEST_MAXYDBDIST_UPDATE_PROCESS,	/* 114 : Make ydb_dist too big for update process */
	WBTEST_MAXYDBDIST_HELPER_PROCESS,	/* 115 : Make ydb_dist too big for helper process */
	WBTEST_QUERY_HANG,			/* 116 : Hangs to verify that statsdb doesn't decrement */
	WBTEST_RELINKCTL_MAX_ENTRIES,		/* 117 : Bring down the maximum number of relink control entries in one file */
	WBTEST_FAKE_BIG_KEY_COUNT,		/* 118 : Fake large increase in mupip load key count to show it does not overflow */
	WBTEST_TEND_GBLJRECTIME_SLEEP,		/* 119 : Sleep in t_end after SET_GBL_JREC_TIME to induce GTM-8332 */
	WBTEST_SIGTSTP_IN_T_QREAD,		/* 120 : Stop ourselves in t_qread to force secshr_db_clnup to clear read state */
	WBTEST_SLAM_SECSHR_ADDRS,		/* 121 : SIGTERM in init_secshr_addrs - verify secshr_db_clnup does not assert */
	WBTEST_MM_CONCURRENT_FILE_EXTEND,	/* 122 : Extend database concurrently in MM */
	WBTEST_SLEEP_IN_MUPIP_REORG_ENCRYPT,	/* 123 : Sleep in mupip_reorg_encrypt() upon releasing crit */
	WBTEST_OPFNZCONVERT_FILE_ACCESS_ERROR,	/* 124 : gtm_strToTitle() returning U_FILE_ACCESS_ERROR error */
	WBTEST_MUEXTRACT_GVCST_RETURN_FALSE,	/* 125 : check return value of gvcst_get in concurrent update. */
	WBTEST_SECSHRWRAP_NOETC,		/* 126 : gtmsecshr_wrapper pretending /etc doesn't exist */
	WBTEST_SECSHRWRAP_NOENVIRON,		/* 127 : gtmsecshr_wrapper pretending /etc/environment doesn't exist */
	WBTEST_SECSHRWRAP_NOTZREC_READERR,	/* 128 : gtmsecshr_wrapper read error */
	WBTEST_SECSHRWRAP_NOTZREC_EOF,		/* 129 : gtmsecshr_wrapper no TZ record found in environment file */
	WBTEST_SECSHRWRAP_SETENVFAIL1,		/* 130 : gtmsecshr_wrapper pretend setenv failed (before clearenv) */
	WBTEST_SECSHRWRAP_SETENVFAIL2,		/* 131 : gtmsecshr_wrapper pretend setenv failed (after clearenv) */
	WBTEST_HELPOUT_FAILGROUPCHECK,		/* 132 : Create GTM-8654 : EUID and file owners are not group members */
	WBTEST_LVMON_PSEUDO_FAIL,		/* 133 : Create condition to cause lvmonitor to fail a comparison and report it */
	WBTEST_REPLINSTSTNDALN,			/* 134 : Expected case of REPLINSTSTNDALN error */
	WBTEST_FORCE_SHMPLRECOV,		/* 135 : Always invoke SHMPLRECOV abandoned block processing */
	WBTEST_GETPWUID_CHECK_OVERWRITE,	/* 136 : Check for getpwuid_struct variable overwrite condition */
	WBTEST_NO_REPLINSTMULTI_FAIL,		/* 137 : Unless specified tests should not fail with REPLMULTINSTUPDATE */
	WBTEST_DOLLARDEVICE_BUFFER,		/* 138 : Force larger error messages for $device to exceed DD_BUFLEN */
	WBTEST_LOWERED_JNLEPOCH,		/* 139 : Force larger error messages for $device to exceed DD_BUFLEN */
	WBTEST_SIGTERM_IN_JOB_CHILD,		/* 140 : Generate Sigterm  by killing ourselves before the child fork */
	WBTEST_DB_BLOCKS_WARN,			/* 141 : Force approaching X% of total block limit warning */
	WBTEST_JNL_WRITE_HANG,			/* 142 : Hang after a number of journal writes */
	WBTEST_DB_WRITE_HANG,			/* 143 : Hang after a number of database writes */
	WBTEST_EXPECT_IO_HANG,			/* 144 : Like the above two cases, but using external tools to cause the hang. */
	WBTEST_MURUNDOWN_KILLCMT06,		/* 145 : Kill an update process in CMT06, to test a special case */
	WBTEST_TRASH_HASH_RECOVER,		/* 146 : Fill hash table with junk and recover */
	WBTEST_TRASH_HASH_NO_RECOVER,		/* 147 : Fill hash table with junk */
	WBTEST_MLOCK_HANG,			/* 148 : Hang after getting mlock so something can issue MUPIP STOP on us */
	WBTEST_MLOCK_HANG_AFTER_SCAN,		/* 149 : Hang after scanning pids so something can change root block on us */
	WBTEST_FORCE_WCS_GET_SPACE_CACHEVRFY,	/* 150 : Force wcs_get_space() to attempt cache verification */
	WBTEST_LOCK_HASH_ZTW,			/* 151 : Put the hash of each LOCK command's final node in $ZTWORMHOLE */
	WBTEST_LOCK_HASH_OFLOW,			/* 152 : Allow hash neighborhood overflow without complaint */
	WBTEST_ZTIMEOUT_TRACE,			/* 153 : Trace the setting/deferring of ztimeout when trigerred */
	WBTEST_ZTIME_DEFER_CRIT,		/* 154 : Defer ZTIMEOUT when CRIT is held */
	WBTEST_LOW_MEMORY,			/* 155 : In tests that limit vmemuse, exit gracefully when system calls,
						 * external libraries, etc.  fail to allocate more memory */
	WBTEST_JNLPROCSTUCK_FORCE,		/* 156 : Mess with jnl_sub_write_attempt to force a JNLPROCSTUCK message */
	WBTEST_JNLEXT_PREREMOVED,		/* 157 : During a MUR_JNLEXT_UNLINK return status that the file does not exist.*/
	WBTEST_REPLCOMM_ERR,			/* 158 : Reconnect in case of replicaton communication errors */
	WBTEST_REPL_INIT_ERR,			/* 159 : Error in setting up the sockets */
	WBTEST_REPL_INIT_ERR2,			/* 160 : Error at listen */
	WBTEST_REPLCOMM_ERR_SRC,		/* 161 : Communication error in recv source server side */
	WBTEST_REPLCOMM_SEND_SRC,		/* 162 : Communication error in send at source side */
	WBTEST_SOCKET_KEEPALIVE,		/* 163 : shorten keepalive parameters so test fails fast */
	WBTEST_FETCHCOMM_ERR,			/* 164 : Communication error during fetchresync rollback */
	WBTEST_FETCHCOMM_HISTINFO,		/* 165 : Communication error during fetchresync rollback HISTINFO */
	WBTEST_MUINTEG_TQREAD,			/* 166 : Force t_qread to return NULL. To test integ with frozen instance */
	WBTEST_FULLBLKWRT_DB,			/* 167 : Full disk blk writes for new blocks */
	WBTEST_OPENFILE_DB,			/* 168 : Make OPENFILE_DB fail on a RW db */
	WBTEST_REPL_TLS_RECONN,			/* 169 : Try reconnect in case of transient TLS errors */
	WBTEST_ZTIM_EDGE,			/* 170 : Timeout when trying to reset the timer */
	WBTEST_LCKWAKEOVRFLO,			/* 171 : Force LOCK wakeups to almost overflow */
	WBTEST_FAKE_SS_SHMDT_WINDOW,		/* 172 : skip lcl_ss_ctx->shdw_fd = FD_INVALID to create "double" SHMDT */
	WBTEST_SOCKET_NONBLOCK,			/* 173 : Set SO_SNDBUF for non blocking socket */
	WBTEST_JOBFAIL_FILE_LIM,		/* 174 : starve the process of file descriptors to produce a JOBFAIL */
	WBTEST_BACKUP_FORCE_MV_RV,		/* 175 : Force error during MUPIP BACKUP mv phase */
	WBTEST_RTNOBJ_INTEG,			/* 176 : Inject a loop into the relinkctl linked list to trigger rtnobj_integ() */
	WBTEST_INDUCE_TLSIOERR,			/* 177 : Source Server does not close the network socket in error shutdown */
	WBTEST_MU_STOP_THREAD,			/* 178 : Will sleep in gtm_multi_thread to let us verify that first two MUPIP STOPs
                                                 *       are indeed deferred in the interrupt-deferred zone, but the third isn't */

	/* Note 1: when adding new white box test cases, please make use of WBTEST_ENABLED and WBTEST_ASSIGN_ONLY (defined below)
	 * whenever applicable
	 * Note 2: when adding a new white box test case, see if an existing WBTEST_UNUSED* slot can be leveraged.
	 */
	/* Start section for YDB-only white-box test cases with a higher number so we do not interfere with
	 * GT.M additions to the above list of cases.
	 */
	WBTEST_YDB_KILL_TERMINAL = 400,		/* 400 : Terminal is killed so accept errors due to this */
	WBTEST_YDB_FILEDELFAIL,			/* 401 : Exercise FILEDELFAIL error codepath in "cre_jnl_file_intrpt_rename" */
	WBTEST_YDB_RENAMEFAIL,			/* 402 : Exercise RENAMEFAIL error codepath in "cre_jnl_file_intrpt_rename" */
	WBTEST_YDB_STATICPID,			/* 403 : Set pid to a static number for predictable test output */
	WBTEST_YDB_SETSTATSOFF,			/* 404 : Reset all the gvstats to be the same as their offset in gvstats_rec_t */
	WBTEST_YDB_MAXSECSHRRETRY,		/* 405 : Cause loop gated by MAX_COMM_ATTEMPTs in secshr_client.c to fail */
	WBTEST_YDB_RLSIGLONGJMP,                /* 406 : Slow down processing of SIGALRM timers to illustrate multiple signal processing issues with readline */
	WBTEST_YDB_ENCR_ENV_TOOLONG,            /* 407 : Encryption env vars are set to too long values (v53003/D9I10002703 subtest) */
	WBTEST_CONCBKUP_RUNNING,		/* 408 : Sleep in mupip_backup to test concurrent BACKUPs */
} wbtest_code_t;

#if defined (DEBUG) && !defined (STATIC_ANALYSIS)
/* Make sure to setenv ydb_white_box_test_case_count if you are going to use GTM_WHITE_BOX_TEST */
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)						\
{													\
	char			count_num[20];								\
	if (ydb_white_box_test_case_enabled && (ydb_white_box_test_case_number == input_test_case_num))	\
	{												\
		gtm_wbox_input_test_case_count++;							\
		/* Catch gtm_wbox_input_test_case_count before reset to convey it to the TLS plugin */	\
		if (WBTEST_REPL_TLS_RECONN == input_test_case_num)					\
		{											\
			snprintf(count_num, SIZEOF(count_num), "%d", gtm_wbox_input_test_case_count);	\
			setenv("wbox_count", count_num, 1);						\
		}											\
		if (ydb_white_box_test_case_count == gtm_wbox_input_test_case_count)			\
		{											\
			lhs = rhs;									\
			gtm_wbox_input_test_case_count = 0;						\
		}											\
	}												\
}
#define WBTEST_ONLY(WBTEST_NUMBER, ...)									\
{													\
	if (WBTEST_ENABLED(WBTEST_NUMBER))								\
	{												\
		__VA_ARGS__										\
	}												\
}
#else
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)
#define WBTEST_ONLY(WBTEST_NUMBER, ...)
#endif

#if defined (DEBUG) && !defined (STATIC_ANALYSIS)
#define WBTEST_ENABLED(WBTEST_NUMBER)	(ydb_white_box_test_case_enabled && (WBTEST_NUMBER == ydb_white_box_test_case_number))
#define ENABLE_WBTEST_ABANDONEDKILL									\
{													\
	int	sleep_counter;										\
													\
	sleep_counter = 0;										\
	GTM_WHITE_BOX_TEST(WBTEST_ABANDONEDKILL, sleep_counter, SLEEP_ONE_MIN);				\
	if (SLEEP_ONE_MIN == sleep_counter)								\
	{												\
		assert(ydb_white_box_test_case_enabled);						\
		util_out_print("!/INFO : WBTEST_ABANDONEDKILL waiting in Phase II of Kill",TRUE);	\
		while (1 <= sleep_counter)								\
			wcs_sleep(sleep_counter--);							\
	}												\
}
#define WB_PHASE1_COMMIT_ERR	(WBTEST_BG_UPDATE_BTPUTNULL == ydb_white_box_test_case_number)
#define WB_PHASE2_COMMIT_ERR	(WBTEST_BG_UPDATE_PHASE2FAIL == ydb_white_box_test_case_number)
#define WB_COMMIT_ERR_ENABLED	(WB_PHASE1_COMMIT_ERR || WB_PHASE2_COMMIT_ERR)	/* convoluted definition to simplify usage */
#define WBTEST_ASSIGN_ONLY(WBTEST_NUMBER, LHS, RHS)							\
{													\
	if (WBTEST_ENABLED(WBTEST_NUMBER))								\
	{												\
		LHS = RHS;										\
	}												\
}
#else
#define WB_PHASE2_COMMIT_ERR	FALSE
#define WBTEST_ENABLED(WBTEST_NUMBER)	FALSE
#define ENABLE_WBTEST_ABANDONEDKILL
#ifdef DEBUG
#define WB_COMMIT_ERR_ENABLED		FALSE
#else
#define WB_COMMIT_ERR_ENABLED
#endif
#define WBTEST_ASSIGN_ONLY(WBTEST_NUMBER, LHS, RHS)
#endif

#endif
