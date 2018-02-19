/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SECSHR_DB_CLNUP
#define SECSHR_DB_CLNUP

enum secshr_db_state
{
	NORMAL_TERMINATION = 1,		/* normal shut down */
	COMMIT_INCOMPLETE		/* in the midst of commit. cannot be rolled back anymore. only rolled forward */
};

enum secshr_accounting_caller_t		/* abbreviated as "sac_" prefix below */
{
	sac_secshr_db_clnup = 1,		/* 1 */
	sac_secshr_finish_CMT08_to_CMT14,	/* 2 */
	sac_secshr_blk_full_build,		/* 3 */
	sac_secshr_finish_CMT18,		/* 4 */
};

#define	IS_EXITING_FALSE	FALSE
#define	IS_EXITING_TRUE		TRUE

#define	IS_REPL_REG_FALSE	FALSE
#define	IS_REPL_REG_TRUE	TRUE

#define	WCBLOCKED_NOW_CRIT_LIT		"wcb_secshr_db_clnup_now_crit"
#define	WCBLOCKED_WBUF_DQD_LIT		"wcb_secshr_db_clnup_wbuf_dqd"
#define	WCBLOCKED_PHASE2_CLNUP_LIT	"wcb_secshr_db_clnup_phase2_clnup"

#define	SECSHR_SET_CSD_CNL_ISBG(CSA, CSD, CNL, IS_BG)				\
MBSTART {									\
	CSD = CSA->hdr;								\
	assert(NULL != CSD);							\
	CNL = CSA->nl;								\
	assert(NULL != CNL);							\
	IS_BG = (CSD->acc_meth == dba_bg);					\
	csa->ti = &csd->trans_hist;	/* correct it in case broken */		\
} MBEND

#define	SECSHR_ACCOUNTING_MAX_ARGS	32	/* max args in "argarray" passed to "secshr_send_DBCLNUPINFO_msg" */

#define		SECSHR_ACCOUNTING(NUMARGS, ARGARRAY, VALUE)	\
MBSTART {							\
	ARGARRAY[NUMARGS++] = (INTPTR_T)#VALUE;			\
	ARGARRAY[NUMARGS++] = VALUE;				\
	assert(NUMARGS <= ARRAYSIZE(ARGARRAY));			\
} MBEND

#ifdef DEBUG_CHECK_LATCH
#  define DEBUG_LATCH(x) x
#else
#  define DEBUG_LATCH(x)
#endif

#define	RELEASE_LATCH_IF_OWNER(X)						\
MBSTART {									\
	if ((X)->u.parts.latch_pid == process_id)				\
	{									\
		SET_LATCH_GLOBAL(X, LOCK_AVAILABLE);				\
		DEBUG_LATCH(util_out_print("Latch cleaned up", FLUSH));		\
	}									\
} MBEND

void	secshr_db_clnup(enum secshr_db_state state);
void	secshr_finish_CMT08_to_CMT14(sgmnt_addrs *csa, jnlpool_addrs_ptr_t update_jnlpool);
void	secshr_rel_crit(gd_region *reg, boolean_t is_exiting, boolean_t is_repl_reg);
int	secshr_blk_full_build(boolean_t is_tp, sgmnt_addrs *csa,
		 sgmnt_data_ptr_t csd, boolean_t is_bg, struct cw_set_element_struct *cs, sm_uc_ptr_t blk_ptr, trans_num currtn);
int	secshr_finish_CMT18(sgmnt_addrs *csa, sgmnt_data_ptr_t csd,
			boolean_t is_bg, struct cw_set_element_struct *cs, sm_uc_ptr_t blk_ptr, trans_num currtn);
void	secshr_finish_CMT18_to_CMT19(sgmnt_addrs *csa);
void	secshr_send_DBCLNUPINFO_msg(sgmnt_addrs *csa, int numargs, gtm_uint64_t *argarray);

#endif
