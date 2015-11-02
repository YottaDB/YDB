/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __WCS_FLU_H__
#define __WCS_FLU_H__

bool wcs_flu(bool options);

#define	SET_WCS_FLU_FAIL_STATUS(status, csd)								\
{	/* Only reason we currently know why wcs_flu can fail (when called from t_end or tp_tend)	\
	 * is if wcs_flu avoided invoking wcs_recover because csd->wc_blocked is already set to TRUE.	\
	 * Possible only if cache-recoveries are induced by white-box testing. Assert accordingly.	\
	 */												\
	assert(csd->wc_blocked);									\
	assert(gtm_white_box_test_case_enabled);							\
	assert(CDB_STAGNATE >= t_tries);								\
	if ((dba_bg == csd->acc_meth) && (CDB_STAGNATE <= t_tries))					\
	{	/* We are in final retry but have to restart because some other process encountered	\
		 * an error in phase2 of commit and has set wc_blocked to TRUE causing us (in-crit	\
		 * process) not to be able to flush the cache. We do not want to increase t_tries in 	\
		 * this case as that will cause us to error out of the transaction. Instead treat this	\
		 * like a helped out case. This will cause us to retry the transaction and as part of	\
		 * that we will perform a cache-recovery that should reset csd->wc_blocked to FALSE.	\
		 * This should cause the wcs_flu() done in the next retry to succeed unless yet another	\
		 * process set wc_blocked to TRUE as part of its phase2 commit. In the worst case we	\
		 * could restart as many times as there are processes concurrently running phase2 	\
		 * commits. Since we dont release crit throughout this final-retry restart loop, we	\
		 * are guaranteed not to do infinite retries.						\
		 */											\
		status = (enum cdb_sc)cdb_sc_helpedout;							\
	} else												\
		status = (enum cdb_sc)cdb_sc_cacheprob;							\
}

#endif
