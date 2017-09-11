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

#ifndef __SECSHR_DB_CLNUP_
#define __SECSHR_DB_CLNUP_

enum secshr_db_state
{
	ABNORMAL_TERMINATION = 0,	/* abnormal shut down using STOP/ID in VMS. Currently unused in Unix */
	NORMAL_TERMINATION,		/* normal shut down */
	COMMIT_INCOMPLETE		/* in the midst of commit. cannot be rolled back anymore. only rolled forward */
};

void secshr_db_clnup(enum secshr_db_state state);

/* SECSHR_ACCOUNTING macro assumes csa->nl is dereferencible and does accounting if variable "DO_ACCOUNTING" is set to TRUE */
#define		SECSHR_ACCOUNTING(DO_ACCOUNTING, VALUE)							\
MBSTART {												\
	if (DO_ACCOUNTING)										\
	{												\
		if (csa->nl->secshr_ops_index < SECSHR_OPS_ARRAY_SIZE)					\
			csa->nl->secshr_ops_array[csa->nl->secshr_ops_index] = (gtm_uint64_t)(VALUE);	\
		csa->nl->secshr_ops_index++;								\
	}												\
} MBEND

#endif
