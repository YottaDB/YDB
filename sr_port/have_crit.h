/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef HAVE_CRIT_H_INCLUDED
#define HAVE_CRIT_H_INCLUDED

/* states of CRIT passed as argument to have_crit() */
#define CRIT_IN_COMMIT		0x00000001
#define CRIT_NOT_TRANS_REG	0x00000002
#define CRIT_RELEASE		0x00000004
#define CRIT_ALL_REGIONS	0x00000008

#define	CRIT_IN_WTSTART		0x00000010	/* check if csa->in_wtstart is true */

/* Note absence of any flags is default value which finds if any region
   or the replication pool have crit or are getting crit. It returns
   when one is found without checking further.
*/
#define CRIT_HAVE_ANY_REG	0x00000000

typedef enum
{
	INTRPT_OK_TO_INTERRUPT = 0,
	INTRPT_IN_GTCMTR_TERMINATE,
	INTRPT_IN_TP_UNWIND,
	INTRPT_IN_TP_CLEAN_UP,
	INTRPT_IN_CRYPT_SECTION,
	INTRPT_IN_DB_CSH_GETN,
	INTRPT_IN_DB_INIT,
	INTRPT_IN_GDS_RUNDOWN,
	INTRPT_IN_SS_INITIATE,
	INTRPT_IN_ZLIB_CMP_UNCMP,
	INTRPT_NUM_STATES
} intrpt_state_t;

GBLREF	intrpt_state_t	intrpt_ok_state;

/* Macro to check if we are in a state that is ok to interrupt (or to do deferred signal handling).
 * We do not want to interrupt if the global variable intrpt_ok_state indicates it is not ok to interrupt,
 * if we are in the midst of a malloc, if we are holding crit, if we are in the midst of commit, or in
 * wcs_wtstart. In the last case, we could be causing another process HOLDING CRIT on the region to wait
 * in bg_update_phase1 if we hold the write interlock. Hence it is important for us to finish that as soon
 * as possible and not interrupt it.
 */
#define	OK_TO_INTERRUPT	((INTRPT_OK_TO_INTERRUPT == intrpt_ok_state) && (0 == gtmMallocDepth)			\
				&& (0 == have_crit(CRIT_HAVE_ANY_REG | CRIT_IN_COMMIT | CRIT_IN_WTSTART)))

/* Macro to be used whenever we want to handle any signals that we deferred handling and exit in the process.
 * In VMS, we dont do any signal handling, only exit handling.
 */
#define	DEFERRED_EXIT_HANDLING_CHECK									\
{													\
	VMS_ONLY(GBLREF	int4	exi_condition;)								\
	GBLREF	int		process_exiting;							\
	GBLREF	VSIG_ATOMIC_T	forced_exit;								\
	GBLREF	volatile int4	gtmMallocDepth;								\
													\
	if (forced_exit && !process_exiting && OK_TO_INTERRUPT)						\
		UNIX_ONLY(deferred_signal_handler();)							\
		VMS_ONLY(sys$exit(exi_condition);)							\
}

#define SAVE_INTRPT_OK_STATE(NEWSTATE)									\
{													\
	save_intrpt_ok_state = intrpt_ok_state;								\
	intrpt_ok_state = NEWSTATE;									\
}

#define RESTORE_INTRPT_OK_STATE										\
{													\
	intrpt_ok_state = save_intrpt_ok_state;								\
	DEFERRED_EXIT_HANDLING_CHECK;	/* check if any signals were deferred while we held lock */	\
}

uint4 have_crit(uint4 crit_state);

#endif /* HAVE_CRIT_H_INCLUDED */
