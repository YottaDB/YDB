/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
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

#endif
