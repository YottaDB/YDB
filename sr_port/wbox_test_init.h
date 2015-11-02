
/****************************************************************
 *								*
 *	Copyright 2005, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WBOX_TEST_INIT_
#define __WBOX_TEST_INIT_

GBLREF	boolean_t	gtm_white_box_test_case_enabled;
GBLREF	int		gtm_white_box_test_case_number;
GBLREF	int		gtm_white_box_test_case_count;
GBLREF	int 		gtm_wbox_input_test_case_count;

void wbox_test_init(void);

/* List of whitebox testcases */
typedef enum {
	WBTEST_T_END_JNLFILOPN = 1,		/*  1 */
	WBTEST_TP_TEND_JNLFILOPN,		/*  2 */
	WBTEST_TP_TEND_TRANS2BIG,		/*  3 */
	WBTEST_BG_UPDATE_BTPUTNULL,		/*  4 */
	WBTEST_BG_UPDATE_DBCSHGET_INVALID,	/*  5 */
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID,	/*  6 */
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID2,	/*  7 : VMS only twin logic */
	WBTEST_BG_UPDATE_READINPROGSTUCK1,	/*  8 */
	WBTEST_BG_UPDATE_READINPROGSTUCK2,	/*  9 */
	WBTEST_BG_UPDATE_DIRTYSTUCK1,		/* 10 : Unix only dirty wait logic */
	WBTEST_BG_UPDATE_DIRTYSTUCK2,		/* 11 */
	WBTEST_BG_UPDATE_INTENDSTUCK,		/* 12 */
	WBTEST_BG_UPDATE_INSQTIFAIL,		/* 13 */
	WBTEST_BG_UPDATE_INSQHIFAIL,		/* 14 */
	WBTEST_BG_UPDATE_PHASE2FAIL,		/* 15 */
	WBTEST_JNL_FILE_LOST_DSKADDR,		/* 16 */
	WBTEST_REPL_HEARTBEAT_NO_ACK,		/* 17 : Unix only */
	WBTEST_REPL_TEST_UNCMP_ERROR,		/* 18 : Unix only */
	WBTEST_REPL_TR_UNCMP_ERROR,		/* 19 : Unix only */
	WBTEST_TP_HIST_CDB_SC_BLKMOD,		/* 20 */
	WBTEST_ABANDONEDKILL,			/* 21 : MUPIP STOP a kill in progress in 2nd stage*/
	WBTEST_ENCRYPT_INIT_ERROR,		/* 22 : Prevent encryption initialized assert from happening */
	WBTEST_UPD_PROCESS_ERROR,		/* 23 : Update process should issue GVSUBOFLOW error, REC2BIG error */
	WBTEST_FILE_EXTEND_ERROR,		/* 24 : Prevent assert form mupip extend if # blocks is > 224M */
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
	WBTEST_CMP_SOLVE_TIMEOUT,		/* 46 : !!! UNUSED !!! */
	WBTEST_SENDTO_EPERM,			/* 47 : Will sleep in grab_crit depending on gtm_white_box_test_case_number */
	WBTEST_ALLOW_ARBITRARY_FULLY_UPGRADED,	/* 48 : Allows csd->fully_upgraded to take arbitrary values (via DSE) and prevents
						 *      assert in mur_process_intrpt_recov.c */
	WBTEST_HOLD_ONTO_FTOKSEM_IN_DBINIT,	/* 49 : Sleep in db_init after getting hold of the ftok semaphore */
	WBTEST_HOLD_ONTO_ACCSEM_IN_DBINIT,	/* 50 : Sleep in db_init after getting hold of the access control semaphore */
	WBTEST_JNL_SWITCH_EXPECTED,		/* 51 : We expect an automatic journal file switch in jnl_file_open */
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
	WBTEST_SET_WC_BLOCKED,			/* 62 : Set the wc_blocked when searching the tree to start wcs_recover process*/
	WBTEST_CLOSE_JNLFILE,			/* 63 : Set the journal file state to close when reading journal files to
						 *	trigger repl_warn message */
	WBTEST_WCS_FLU_IOERR,			/* 64 : Force an I/O error (other than ENOSPC) when wcs_wtstart is invoked from
						 *      wcs_flu */
	WBTEST_WCS_WTSTART_IOERR,		/* 65 : Force an I/O error (other than ENOSPC) within wcs_wtstart */
	WBTEST_HOLD_CRIT_TILL_LCKALERT,		/* 66 : Grab and hold crit until 15 seconds past what triggers a lock alert message
						 *      which should invoke a mutex salvage */
	WBTEST_OPER_LOG_MSG,			/* 67 : send message to operator log */
	WBTEST_ANTIFREEZE_DSKNOSPCAVAIL,	/* 68 : */
	WBTEST_ANTIFREEZE_JNLCLOSE,		/* 69 : */
	WBTEST_ANTIFREEZE_DBBMLCORRUPT,		/* 70 : */
	WBTEST_ANTIFREEZE_DBDANGER,		/* 71 : */
	WBTEST_ANTIFREEZE_DBFSYNCERR,		/* 72 :	*/
	WBTEST_ANTIFREEZE_GVDATAFAIL,		/* 73 : */
	WBTEST_ANTIFREEZE_GVGETFAIL,		/* 74 : */
	WBTEST_ANTIFREEZE_GVINCRPUTFAIL,	/* 75 : */
	WBTEST_ANTIFREEZE_GVKILLFAIL,		/* 76 : */
	WBTEST_ANTIFREEZE_GVORDERFAIL,		/* 77 : */
	WBTEST_ANTIFREEZE_GVQUERYFAIL,		/* 78 : */
	WBTEST_ANTIFREEZE_GVQUERYGETFAIL,	/* 79 : */
	WBTEST_ANTIFREEZE_GVZTRIGFAIL,		/* 80 : */
	WBTEST_ANTIFREEZE_OUTOFSPACE		/* 81 : */
} wbtest_code_t;

#ifdef DEBUG
/* Make sure to setenv gtm_white_box_test_case_count if you are going to use GTM_WHITE_BOX_TEST */
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)						\
{													\
	if (gtm_white_box_test_case_enabled && (gtm_white_box_test_case_number == input_test_case_num))	\
	{												\
		gtm_wbox_input_test_case_count++;							\
		if (gtm_white_box_test_case_count == gtm_wbox_input_test_case_count)			\
		{											\
			lhs = rhs;									\
			gtm_wbox_input_test_case_count = 0;						\
		}											\
	}												\
}
#else
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)
#endif

#ifdef DEBUG
#define ENABLE_WBTEST_ABANDONEDKILL									\
{													\
	int	sleep_counter;										\
													\
	sleep_counter = 0;										\
	GTM_WHITE_BOX_TEST(WBTEST_ABANDONEDKILL, sleep_counter, SLEEP_ONE_MIN);				\
	if (SLEEP_ONE_MIN == sleep_counter)								\
	{												\
		assert(gtm_white_box_test_case_enabled);						\
		util_out_print("!/INFO : WBTEST_ABANDONEDKILL waiting in Phase II of Kill",TRUE);	\
		while (1 <= sleep_counter)								\
			wcs_sleep(sleep_counter--);							\
	}												\
}
#define WB_PHASE1_COMMIT_ERR	(WBTEST_BG_UPDATE_BTPUTNULL == gtm_white_box_test_case_number)
#define WB_PHASE2_COMMIT_ERR	(WBTEST_BG_UPDATE_PHASE2FAIL == gtm_white_box_test_case_number)
#define WB_COMMIT_ERR_ENABLED	(WB_PHASE1_COMMIT_ERR || WB_PHASE2_COMMIT_ERR)	/* convoluted definition to simplify usage */
#else
#define ENABLE_WBTEST_ABANDONEDKILL
#define WB_COMMIT_ERR_ENABLED
#endif

#endif

