/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef T_COMMIT_CLEANUP_DEFINED
#define T_COMMIT_CLEANUP_DEFINED

boolean_t t_commit_cleanup(enum cdb_sc status, int signal);

#define	INVOKE_T_COMMIT_CLEANUP(status, csa)								\
{													\
	boolean_t	retvalue;									\
													\
	assert(gtm_white_box_test_case_enabled);							\
	retvalue = t_commit_cleanup(status, 0);								\
	/* return value of TRUE implies	secshr_db_clnup has done commit for us */			\
	assert(retvalue);										\
	/* reset status to normal as transaction is now complete */					\
	status = cdb_sc_normal;										\
	assert(!csa->now_crit || csa->hold_onto_crit);	/* shouldn't hold crit unless asked to */	\
	assert(!csa->t_commit_crit); /* assert we are no longer in midst of commit */			\
}

#endif
