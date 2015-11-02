
/****************************************************************
 *								*
 *	Copyright 2005, 2007 Fidelity Information Services, Inc	*
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
	WBTEST_T_END_JNLFILOPN = 1,
	WBTEST_TP_TEND_JNLFILOPN,
	WBTEST_TP_TEND_TRANS2BIG,
	WBTEST_BG_UPDATE_BTPUTNULL,
	WBTEST_BG_UPDATE_DBCSHGET_INVALID,
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID,
	WBTEST_BG_UPDATE_DBCSHGETN_INVALID2,	/* VMS only twin logic */
	WBTEST_BG_UPDATE_READINPROGSTUCK1,
	WBTEST_BG_UPDATE_READINPROGSTUCK2,
	WBTEST_BG_UPDATE_DIRTYSTUCK1,		/* Unix only dirty wait logic */
	WBTEST_BG_UPDATE_DIRTYSTUCK2
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

