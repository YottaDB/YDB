
/****************************************************************
 *								*
 *	Copyright 2005 Fidelity Information Services, Inc	*
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
#define	WBTEST_T_END_JNLFILOPN			 1
#define	WBTEST_TP_TEND_JNLFILOPN		 2
#define	WBTEST_TP_TEND_TRANS2BIG		 3
#define	WBTEST_BG_UPDATE_BTPUTNULL		 4
#define	WBTEST_BG_UPDATE_DBCSHGET_INVALID	 5
#define	WBTEST_BG_UPDATE_DBCSHGETN_INVALID	 6
#define	WBTEST_BG_UPDATE_DBCSHGETN_INVALID2	 7	/* VMS only twin logic */
#define	WBTEST_BG_UPDATE_READINPROGSTUCK1	 8
#define	WBTEST_BG_UPDATE_READINPROGSTUCK2	 9
#define	WBTEST_BG_UPDATE_DIRTYSTUCK1		10	/* Unix only dirty wait logic */
#define	WBTEST_BG_UPDATE_DIRTYSTUCK2		11

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

