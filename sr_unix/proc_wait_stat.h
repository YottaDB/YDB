/****************************************************************
 *								*
 * Copyright (c) 2019-2020 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef PROC_WAIT_STAT
#define PROC_WAIT_STAT

/* This code tracks the entrance/exit of the various crit states and keeps the statistic counters
 * (including those built up from components) updated.  During white box testing, it calls a function
 * to coordinate with test scripts.  States which are visible to the user have names and comments. */
typedef enum
{
	WS_1,
	WS_2,	/* JOPA: wait flag for journal open in prog */
	WS_3,
	WS_4,
	WS_5,
	WS_6,
	WS_7,
	WS_8,
	WS_9,
	WS_10,
	WS_11,
	WS_12,	/* AFRA: wait flag for auto freeze release */
	WS_13,
	WS_14,
	WS_15,	/* BREA: wait flag for blk rd encryp cycle sync */
	WS_16,
	WS_17,
	WS_18,
	WS_19,
	WS_20,
	WS_21,
	WS_22,
	WS_23,
	WS_24,
	WS_25,
	WS_26,
	WS_27,
	WS_28,
	WS_29,
	WS_30,
	WS_31,
	WS_32,
	WS_33,
	WS_34,
	WS_35,
	WS_36,
	WS_37,
	WS_38,
	WS_39,	/* MLBA: wait flag for mlk acquire blocked */
	WS_40,
	WS_41,
	WS_42,
	WS_43,
	WS_44,
	WS_45,
	WS_46,
	WS_47,	/* TRGA: wait flag for grab region for trans */
	WS_48,
	WS_49,
	WS_50,
	WS_51,
	WS_52,
	WS_53,
	WS_54,
	WS_55,
	WS_56,
	WS_57,
	WS_58,
	WS_59,
	WS_60,
	WS_61,
	WS_62,
	WS_63,
	WS_64,
	WS_65,
	WS_66,
	WS_67,
	WS_68,
	WS_69,
	WS_70,
	WS_71,
	WS_72,
	WS_73,
	WS_74,
	WS_75,
	WS_76,
	WS_77,
	WS_78,
	WS_79,
	WS_80,
	WS_81,
	WS_82,
	WS_83,
	WS_84,
	WS_85,
	WS_86,
	WS_87,
	WS_88,
	WS_89,
	WS_90,
	WS_91,
	WS_92,
	WS_93,
	WS_94,
	WS_95,
	WS_96,
	WS_97,
	WS_98,
	WS_99,
	WS_100,
	WS_101,
	WS_102,
	NOT_APPLICABLE
} wait_state;

void wb_gtm8863_lock_pause(void *,wait_state,int);

/* INCVAL 1: set, -1 unset */											\
#define UPDATE_PROC_WAIT_STATE(CSADDRS, WAITSTATE, INCVAL)                                                      \
MBSTART {                                                                                                       \
	int DEXA = 0;                                                                                           \
	int GLB = 0;                                                                                            \
	int JNL = 0;                                                                                            \
	int MLK = 0;                                                                                            \
	int PRC = 0;                                                                                            \
	int TRX = 0;                                                                                            \
	int ZAD = 0;                                                                                            \
	assert((1 == INCVAL) || (-1 ==INCVAL));									\
	if (CSADDRS && CSADDRS->nl)                                                                             \
	{                                                                                                       \
		switch(WAITSTATE)                                                                               \
		{                                                                                               \
			case WS_2 :                                                                             \
				(CSADDRS)->gvstats_rec_p->f_ws2 = (1 == INCVAL) ? 1 : 0;			\
				++JNL;                                                                          \
				break;                                                                          \
			case WS_12 :                                                                            \
				(CSADDRS)->gvstats_rec_p->f_ws12 = (1 == INCVAL) ? 1 : 0;			\
				++DEXA;                                                                         \
				break;                                                                          \
			case WS_15 :                                                                            \
				(CSADDRS)->gvstats_rec_p->f_ws15 = (1 == INCVAL) ? 1 : 0;			\
				++GLB;                                                                          \
				break;                                                                          \
			case WS_39 :                                                                            \
				(CSADDRS)->gvstats_rec_p->f_ws39 = (1 == INCVAL) ? 1 : 0;			\
				++MLK;                                                                          \
				break;                                                                          \
			case WS_47 :                                                                            \
				(CSADDRS)->gvstats_rec_p->f_ws47 = (1 == INCVAL) ? 1 : 0;			\
				++TRX;                                                                          \
				break;                                                                          \
			case WS_1 :  /* For aggregate ZAD */                                                    \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_3 :  /* For aggregate JNL */                                                    \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_4 :  /* For aggregate JNL */                                                    \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_5 :  /* For aggregate MLK */                                                    \
				++MLK;                                                                          \
				break;                                                                          \
			case WS_6 :  /* For aggregate ZAD */                                                    \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_7 :  /* For aggregate ZAD */                                                    \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_8 :  /* For aggregate ZAD */                                                    \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_9 :  /* For aggregate ZAD */                                                    \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_10 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_11 :  /* For aggregate DEXA */                                                  \
				++DEXA;                                                                         \
				break;                                                                          \
			case WS_13 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_14 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_16 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_17 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_18 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_19 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_20 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_21 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_22 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_23 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_24 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_25 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_26 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_27 :  /* For aggregate GLB */                                                   \
				++GLB;                                                                          \
				break;                                                                          \
			case WS_28 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_29 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_30 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_31 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_32 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_33 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_34 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_35 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_36 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_37 :  /* For aggregate JNL */                                                   \
				++JNL;                                                                          \
				break;                                                                          \
			case WS_38 :  /* For aggregate MLK */                                                   \
				++MLK;                                                                          \
				break;                                                                          \
			case WS_40 :  /* For aggregate PRC */                                                   \
				++PRC;                                                                          \
				break;                                                                          \
			case WS_41 :  /* For aggregate PRC */                                                   \
				++PRC;                                                                          \
				break;                                                                          \
			case WS_43 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_44 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_45 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_46 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_48 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_49 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_50 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_51 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_52 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_53 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_54 :  /* For aggregate TRX */                                                   \
				++TRX;                                                                          \
				break;                                                                          \
			case WS_55 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_56 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_57 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_58 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_59 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_60 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_61 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_62 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_63 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_64 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_65 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_66 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_67 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_68 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_69 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_70 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_71 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_72 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_73 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_74 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_75 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_76 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_77 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_78 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_79 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_80 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_81 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_82 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_83 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_84 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_85 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_86 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_87 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_88 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_89 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_90 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_91 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_92 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_93 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_94 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_95 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_96 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_97 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_98 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_99 :  /* For aggregate ZAD */                                                   \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_100 :  /* For aggregate ZAD */                                                  \
				++ZAD;                                                                          \
				break;                                                                          \
			case WS_101 :  /* For aggregate ZAD */                                                  \
				++ZAD;                                                                          \
				break;                                                                          \
			default : /* It is not an error for some instrumentation to be ignored */               \
				break;                                                                          \
		}                                                                                               \
		if (0 < DEXA)                                                                                   \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_dbext_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_dbext_wait = (CSADDRS)->gvstats_rec_p->f_dbext_wait;	\
		}                                                                                               \
		if (0 < GLB)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_bg_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_bg_wait = (CSADDRS)->gvstats_rec_p->f_bg_wait;		\
		}                                                                                               \
		if (0 < JNL)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_jnl_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_jnl_wait = (CSADDRS)->gvstats_rec_p->f_jnl_wait;		\
		}                                                                                               \
		if (0 < MLK)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_mlk_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_mlk_wait = (CSADDRS)->gvstats_rec_p->f_mlk_wait;		\
		}                                                                                               \
		if (0 < PRC)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_proc_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_proc_wait = (CSADDRS)->gvstats_rec_p->f_proc_wait;		\
		}                                                                                               \
		if (0 < TRX)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_trans_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_trans_wait = (CSADDRS)->gvstats_rec_p->f_trans_wait;	\
		}                                                                                               \
		if (0 < ZAD)                                                                                    \
		{                                                                                               \
			(CSADDRS)->gvstats_rec_p->f_util_wait = (1 == INCVAL) ? 1 : 0;				\
			(CSADDRS)->nl->gvstats_rec.f_util_wait = (CSADDRS)->gvstats_rec_p->f_util_wait;		\
		}                                                                                               \
		                                                                                                \
		/* We use ydb_white_box_test_case_count here as a WS value.                                     \
		 * The flag file lets us coordinate without timing issues.					\
		 */                                      							\
		WBTEST_ONLY(WBTEST_WSSTATS_PAUSE,                                                               \
		{                                                                                               \
			wb_gtm8863_lock_pause(CSADDRS,WAITSTATE,INCVAL);                                        \
		});                                                                                             \
	}                                                                                                       \
} MBEND

#endif
