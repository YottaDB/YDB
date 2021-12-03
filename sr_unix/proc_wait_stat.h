/****************************************************************
 *								*
 * Copyright (c) 2019-2021 Fidelity National Information	*
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
 * to coordinate with test scripts.  States which are visible to the user have names and comments.
 * This file is based on information from tab_gvstats_rec.h. */
typedef enum
{
	WS_1,	/* (In ZAD) [desired_db_format_set.c] */
	WS_2,	/* JOPA:  (In JNL) [gdsfhead.h] */
	WS_3,	/* (In JNL) [jnl.h] */
	WS_4,	/* (In JNL) [jnl_write_attempt.c] */
	WS_5,	/* (In MLK) [mlk_ops.h] */
	WS_6,	/* (In ZAD) [mu_int_reg.c] */
	WS_7,	/* (In ZAD) [mu_int_wait_rdonly.c] */
	WS_8,	/* (In ZAD) [mu_reorg_upgrd_dwngrd.c] */
	WS_9,	/* (In ZAD) [mu_reorg_upgrd_dwngrd.c] */
	WS_10,	/* (In ZAD) [mupip_backup.c] */
	WS_11,	/* (In DEXA) [wcs_recover.c] */
	WS_12,	/* AFRA:  (In DEXA) [gdsfilext.c] */
	WS_13,	/* (In GLB) [gvcst_expand_free_subtree.c] */
	WS_14,	/* (In GLB) [t_qread.c] */
	WS_15,	/* BREA:  (In GLB) [t_qread.c] */
	WS_16,	/* (In GLB) [t_qread.c] */
	WS_17,	/* (In GLB) [t_qread.c] */
	WS_18,	/* (In GLB) [t_qread.c] */
	WS_19,	/* (In GLB) [updproc.c] */
	WS_20,	/* (In GLB) [verify_queue.c] */
	WS_21,	/* (In GLB) [wcs_get_space.h] */
	WS_22,	/* (In GLB) [wcs_verify.c] */
	WS_23,	/* (In GLB) [aio_shim.c] */
	WS_24,	/* (In GLB) [wcs_flu.c] */
	WS_25,	/* (In GLB) [wcs_wtfini.c] */
	WS_26,	/* (In GLB) [wcs_wtstart.c] */
	WS_27,	/* (In GLB) [wcs_wtstart_fini.c] */
	WS_28,	/* (In JNL) [jnl_phase2_cleanup.c] */
	WS_29,	/* (In JNL) [jnl_write.c] */
	WS_30,	/* (In JNL) [op_fnview.c] */
	WS_31,	/* (In JNL) [op_fnview.c] */
	WS_32,	/* (In JNL) [op_view.c] */
	WS_33,	/* (In JNL) [repl_phase2_cleanup.c] */
	WS_34,	/* (In JNL) [gtmsource_readfiles.c] */
	WS_35,	/* (In JNL) [mutex.c] */
	WS_36,	/* (In JNL) [mutex.c] */
	WS_37,	/* (In JNL) [repl_instance.c] */
	WS_38,	/* (In MLK) [mlk_ops.h] */
	WS_39,	/* MLBA:  (In MLK) [op_lock2.c] */
	WS_40,	/* (In PRC) [gds_rundown.c] */
	WS_41,	/* (In PRC) [gds_rundown.c] */
	WS_42,	/* [gds_rundown.c] */
	WS_43,	/* (In TRX) [t_begin_crit.c] */
	WS_44,	/* (In TRX) [t_commit_cleanup.c] */
	WS_45,	/* (In TRX) [t_end.c] */
	WS_46,	/* (In TRX) [t_end.c] */
	WS_47,	/* TRGA:  (In TRX) [t_end.c] */
	WS_48,	/* (In TRX) [t_retry.c] */
	WS_49,	/* (In TRX) [t_retry.c] */
	WS_50,	/* (In TRX) [t_retry.c] */
	WS_51,	/* (In TRX) [tp_hist.c] */
	WS_52,	/* (In TRX) [tp_restart.c] */
	WS_53,	/* (In TRX) [tp_tend.c] */
	WS_54,	/* (In TRX) [tp_tend.c] */
	WS_55,	/* (In ZAD) [dse.h] */
	WS_56,	/* (In ZAD) [dse_all.c] */
	WS_57,	/* (In ZAD) [dse_all.c] */
	WS_58,	/* (In ZAD) [dse_crit.c] */
	WS_59,	/* (In ZAD) [dse_maps.c] */
	WS_60,	/* (In ZAD) [dse_maps.c] */
	WS_61,	/* (In ZAD) [dse_maps.c] */
	WS_62,	/* (In ZAD) [dse_wcreinit.c] */
	WS_63,	/* (In ZAD) [gdsfhead.h] */
	WS_64,	/* (In ZAD) [mu_reorg_upgrd_dwngrd.c] */
	WS_65,	/* (In ZAD) [mupip_backup.c] */
	WS_66,	/* (In ZAD) [mupip_backup.c] */
	WS_67,	/* (In ZAD) [mupip_extend.c] */
	WS_68,	/* (In ZAD) [mupip_extend.c] */
	WS_69,	/* (In ZAD) [mupip_reorg.c] */
	WS_70,	/* (In ZAD) [mupip_reorg.c] */
	WS_71,	/* (In ZAD) [mupip_set_journal.c] */
	WS_72,	/* (In ZAD) [mur_close_files.c] */
	WS_73,	/* (In ZAD) [mur_open_files.c] */
	WS_74,	/* (In ZAD) [mur_output_record.c] */
	WS_75,	/* (In ZAD) [mur_output_record.c] */
	WS_76,	/* (In ZAD) [op_fnview.c] */
	WS_77,	/* (In ZAD) [op_fnview.c] */
	WS_78,	/* (In ZAD) [op_view.c] */
	WS_79,	/* (In ZAD) [region_freeze.c] */
	WS_80,	/* (In ZAD) [region_freeze.c] */
	WS_81,	/* (In ZAD) [region_freeze.c] */
	WS_82,	/* (In ZAD) [region_freeze.c] */
	WS_83,	/* (In ZAD) [region_freeze.c] */
	WS_84,	/* (In ZAD) [gtmsource_rootprimary_init.c] */
	WS_85,	/* (In ZAD) [mu_extract.c] */
	WS_86,	/* (In ZAD) [mu_extract.c] */
	WS_87,	/* (In ZAD) [mu_rndwn_file.c] */
	WS_88,	/* (In ZAD) [mu_truncate.c] */
	WS_89,	/* (In ZAD) [mubfilcpy.c] */
	WS_90,	/* (In ZAD) [mubinccpy.c] */
	WS_91,	/* (In ZAD) [mupip_reorg_encrypt.c] */
	WS_92,	/* (In ZAD) [mupip_reorg_encrypt.c] */
	WS_93,	/* (In ZAD) [mupip_reorg_encrypt.c] */
	WS_94,	/* (In ZAD) [mupip_reorg_encrypt.c] */
	WS_95,	/* (In ZAD) [mupip_set_file.c] */
	WS_96,	/* (In ZAD) [ss_initiate.c] */
	WS_97,	/* (In ZAD) [ss_initiate.c] */
	WS_98,	/* (In ZAD) [ss_initiate.c] */
	WS_99,	/* (In ZAD) [ss_release.c] */
	WS_100,	/* (In ZAD) [trigger_upgrade.c] */
	WS_101,	/* (In ZAD) [trigger_upgrade.c] */
	WS_102,	/* (In ZAD) [mupip_freeze.c] */
	NOT_APPLICABLE
} wait_state;

void wb_gtm8863_lock_pause(void *,wait_state);

#define UPDATE_CRIT_COUNTER(CSADDRS, WAITSTATE)									\
MBSTART {                                                                                                       \
	if (CSADDRS && CSADDRS->nl)										\
	{                                                                                                       \
		switch(WAITSTATE)                                                                               \
		{                                                                                               \
			case WS_2 :                                                                             \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_ws2, 1);				\
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_jnl_wait, 1);			\
				break;                                                                          \
			case WS_12 :                                                                            \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_ws12, 1);			\
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_dbext_wait, 1);			\
				break;                                                                          \
			case WS_15 :                                                                            \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_ws15, 1);			\
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_bg_wait, 1);			\
				break;                                                                          \
			case WS_39 :                                                                            \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_ws39, 1);			\
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_mlk_wait, 1);			\
				break;                                                                          \
			case WS_47 :                                                                            \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_ws47, 1);			\
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_trans_wait, 1);			\
				break;                                                                          \
			case WS_11 :  /* For aggregate DEXA */                                                  \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_dbext_wait, 1);			\
				break;                                                                          \
			case WS_13 :  /* For aggregate GLB */                                                   \
			case WS_14 :  /* For aggregate GLB */                                                   \
			case WS_16 :  /* For aggregate GLB */                                                   \
			case WS_17 :  /* For aggregate GLB */                                                   \
			case WS_18 :  /* For aggregate GLB */                                                   \
			case WS_19 :  /* For aggregate GLB */                                                   \
			case WS_20 :  /* For aggregate GLB */                                                   \
			case WS_21 :  /* For aggregate GLB */                                                   \
			case WS_22 :  /* For aggregate GLB */                                                   \
			case WS_23 :  /* For aggregate GLB */                                                   \
			case WS_24 :  /* For aggregate GLB */                                                   \
			case WS_25 :  /* For aggregate GLB */                                                   \
			case WS_26 :  /* For aggregate GLB */                                                   \
			case WS_27 :  /* For aggregate GLB */                                                   \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_bg_wait, 1);			\
				break;                                                                          \
			case WS_3 :  /* For aggregate JNL */                                                    \
			case WS_4 :  /* For aggregate JNL */                                                    \
			case WS_28 :  /* For aggregate JNL */                                                   \
			case WS_29 :  /* For aggregate JNL */                                                   \
			case WS_30 :  /* For aggregate JNL */                                                   \
			case WS_31 :  /* For aggregate JNL */                                                   \
			case WS_32 :  /* For aggregate JNL */                                                   \
			case WS_33 :  /* For aggregate JNL */                                                   \
			case WS_34 :  /* For aggregate JNL */                                                   \
			case WS_35 :  /* For aggregate JNL */                                                   \
			case WS_36 :  /* For aggregate JNL */                                                   \
			case WS_37 :  /* For aggregate JNL */                                                   \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_jnl_wait, 1);			\
				break;                                                                          \
			case WS_5 :  /* For aggregate MLK */                                                    \
			case WS_38 :  /* For aggregate MLK */                                                   \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_mlk_wait, 1);			\
				break;                                                                          \
			case WS_40 :  /* For aggregate PRC */                                                   \
			case WS_41 :  /* For aggregate PRC */                                                   \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_proc_wait, 1);			\
				break;                                                                          \
			case WS_43 :  /* For aggregate TRX */                                                   \
			case WS_44 :  /* For aggregate TRX */                                                   \
			case WS_45 :  /* For aggregate TRX */                                                   \
			case WS_46 :  /* For aggregate TRX */                                                   \
			case WS_48 :  /* For aggregate TRX */                                                   \
			case WS_49 :  /* For aggregate TRX */                                                   \
			case WS_50 :  /* For aggregate TRX */                                                   \
			case WS_51 :  /* For aggregate TRX */                                                   \
			case WS_52 :  /* For aggregate TRX */                                                   \
			case WS_53 :  /* For aggregate TRX */                                                   \
			case WS_54 :  /* For aggregate TRX */                                                   \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_trans_wait, 1);			\
				break;                                                                          \
			case WS_1 :  /* For aggregate ZAD */                                                    \
			case WS_6 :  /* For aggregate ZAD */                                                    \
			case WS_7 :  /* For aggregate ZAD */                                                    \
			case WS_8 :  /* For aggregate ZAD */                                                    \
			case WS_9 :  /* For aggregate ZAD */                                                    \
			case WS_10 :  /* For aggregate ZAD */                                                   \
			case WS_55 :  /* For aggregate ZAD */                                                   \
			case WS_56 :  /* For aggregate ZAD */                                                   \
			case WS_57 :  /* For aggregate ZAD */                                                   \
			case WS_58 :  /* For aggregate ZAD */                                                   \
			case WS_59 :  /* For aggregate ZAD */                                                   \
			case WS_60 :  /* For aggregate ZAD */                                                   \
			case WS_61 :  /* For aggregate ZAD */                                                   \
			case WS_62 :  /* For aggregate ZAD */                                                   \
			case WS_63 :  /* For aggregate ZAD */                                                   \
			case WS_64 :  /* For aggregate ZAD */                                                   \
			case WS_65 :  /* For aggregate ZAD */                                                   \
			case WS_66 :  /* For aggregate ZAD */                                                   \
			case WS_67 :  /* For aggregate ZAD */                                                   \
			case WS_68 :  /* For aggregate ZAD */                                                   \
			case WS_69 :  /* For aggregate ZAD */                                                   \
			case WS_70 :  /* For aggregate ZAD */                                                   \
			case WS_71 :  /* For aggregate ZAD */                                                   \
			case WS_72 :  /* For aggregate ZAD */                                                   \
			case WS_73 :  /* For aggregate ZAD */                                                   \
			case WS_74 :  /* For aggregate ZAD */                                                   \
			case WS_75 :  /* For aggregate ZAD */                                                   \
			case WS_76 :  /* For aggregate ZAD */                                                   \
			case WS_77 :  /* For aggregate ZAD */                                                   \
			case WS_78 :  /* For aggregate ZAD */                                                   \
			case WS_79 :  /* For aggregate ZAD */                                                   \
			case WS_80 :  /* For aggregate ZAD */                                                   \
			case WS_81 :  /* For aggregate ZAD */                                                   \
			case WS_82 :  /* For aggregate ZAD */                                                   \
			case WS_83 :  /* For aggregate ZAD */                                                   \
			case WS_84 :  /* For aggregate ZAD */                                                   \
			case WS_85 :  /* For aggregate ZAD */                                                   \
			case WS_86 :  /* For aggregate ZAD */                                                   \
			case WS_87 :  /* For aggregate ZAD */                                                   \
			case WS_88 :  /* For aggregate ZAD */                                                   \
			case WS_89 :  /* For aggregate ZAD */                                                   \
			case WS_90 :  /* For aggregate ZAD */                                                   \
			case WS_91 :  /* For aggregate ZAD */                                                   \
			case WS_92 :  /* For aggregate ZAD */                                                   \
			case WS_93 :  /* For aggregate ZAD */                                                   \
			case WS_94 :  /* For aggregate ZAD */                                                   \
			case WS_95 :  /* For aggregate ZAD */                                                   \
			case WS_96 :  /* For aggregate ZAD */                                                   \
			case WS_97 :  /* For aggregate ZAD */                                                   \
			case WS_98 :  /* For aggregate ZAD */                                                   \
			case WS_99 :  /* For aggregate ZAD */                                                   \
			case WS_100 :  /* For aggregate ZAD */                                                  \
			case WS_101 :  /* For aggregate ZAD */                                                  \
			case WS_102 :  /* For aggregate ZAD */                                                  \
				INCR_GVSTATS_COUNTER(CSADDRS, (CSADDRS)->nl, n_util_wait, 1);			\
				break;                                                                          \
			default : /* It is not an error for some instrumentation to be ignored */               \
				break;                                                                          \
		}                                                                                               \
		                                                                                                \
		/* We use ydb_white_box_test_case_count here as a WS value.                                     \
		 * The flag file lets us coordinate without timing issues.					\
		 */                                      							\
		WBTEST_ONLY(WBTEST_WSSTATS_PAUSE,                                                               \
		{                                                                                               \
			wb_gtm8863_lock_pause(CSADDRS,WAITSTATE);						\
		});                                                                                             \
	}                                                                                                       \
} MBEND

#endif
