
/****************************************************************
 *								*
 *	Copyright 2005, 2008 Fidelity Information Services, Inc	*
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
	WBTEST_ABANDONEDKILL			/* 21 MUPIP STOP a kill in progress in 2nd stage*/
} wbtest_code_t;

#ifdef DEBUG
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)						\
{													\
	if (gtm_white_box_test_case_enabled)								\
	{												\
		if (gtm_white_box_test_case_number == input_test_case_num)				\
		{											\
			gtm_wbox_input_test_case_count++;						\
			if (gtm_white_box_test_case_count == gtm_wbox_input_test_case_count)		\
			{										\
				lhs = rhs;								\
				gtm_wbox_input_test_case_count = 0;					\
			}										\
		}											\
	}												\
}
#else
#define GTM_WHITE_BOX_TEST(input_test_case_num, lhs, rhs)
#endif

#endif

