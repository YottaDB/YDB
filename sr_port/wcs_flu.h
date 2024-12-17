/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
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

#ifndef WCS_FLU_H_INCLUDED
#define WCS_FLU_H_INCLUDED

boolean_t wcs_flu(uint4 options);

#define	SET_WCS_FLU_FAIL_STATUS(status, csd)										\
{	/* Reasons we currently know why wcs_flu can fail (when called from t_end or tp_tend) is if			\
	 * 	a) wcs_flu avoided invoking wcs_recover because cnl->wc_blocked is already set				\
	 * 		to TRUE (this is possible only if cache-recoveries are induced by white-box			\
	 * 		testing).											\
	 * OR	b) if wcs_flu encountered errors in the "jnl_flush" call. The only way we know this			\
	 * 		out-of-design situation can happen is if journal buffer fields are tampered			\
	 * 		with by white-box testing. In this case cnl->wc_blocked need not be TRUE.			\
	 * In either case, white-box testing should be true. Assert accordingly.					\
	 *														\
	 * There is one exception to this and that is in the "v61000/intrpt_wcs_wtstart" subtest			\
	 * (in the YDBTest repo) where the writer process has "ydb_white_box_test_case_enabled" unset			\
	 * but has "ydb_white_box_test_case_number" set to "WBTEST_SLEEP_IN_WCS_WTSTART". Account for			\
	 * that with the "||" condition below.										\
	 */														\
	assert(ydb_white_box_test_case_enabled || (WBTEST_SLEEP_IN_WCS_WTSTART == ydb_white_box_test_case_number));	\
	assert(CDB_STAGNATE >= t_tries);										\
	SET_CACHE_FAIL_STATUS(status, csd);										\
}

#define SET_CACHE_FAIL_STATUS(status, csd)								\
{													\
	if ((CDB_STAGNATE <= t_tries) && (dba_bg == csd->acc_meth))					\
	{	/* We are in final retry but have to restart because some other process encountered	\
		 * an error in phase2 of commit and has set wc_blocked to TRUE causing us (in-crit	\
		 * process) not to be able to flush the cache. We do not want to increase t_tries in 	\
		 * this case as that will cause us to error out of the transaction. Instead treat this	\
		 * like a helped out case. This will cause us to retry the transaction and as part of	\
		 * that we'll perform a cache-recovery that should reset cnl->wc_blocked to FALSE.	\
		 * This should cause the wcs_flu() done in the next retry to succeed unless yet another	\
		 * process set wc_blocked to TRUE as part of its phase2 commit. In the worst case we	\
		 * could restart as many times as there are processes concurrently running phase2 	\
		 * commits. Since we dont release crit throughout this final-retry restart loop, we	\
		 * are guaranteed not to do infinite retries.						\
		 *											\
		 * One exception to this is the "v61000/intrpt_wcs_wtstart" subtest just like it is	\
		 * described in comments under the SET_WCS_FLU_FAIL_STATUS macro so handle that below	\
		 * with a "||" condition.								\
		 */											\
		assert((ydb_white_box_test_case_enabled							\
			&& (WB_PHASE2_COMMIT_ERR							\
				|| (WBTEST_JNL_FILE_LOST_DSKADDR == ydb_white_box_test_case_number)))	\
			|| (WBTEST_SLEEP_IN_WCS_WTSTART == ydb_white_box_test_case_number));		\
		status = (enum cdb_sc)cdb_sc_helpedout;							\
	} else												\
		status = (enum cdb_sc)cdb_sc_cacheprob;							\
}

#endif
