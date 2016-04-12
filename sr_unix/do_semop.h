/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef DO_SEMOP_INCLUDED
#define DO_SEMOP_INCLUDED

error_def(ERR_NOMORESEMCNT);

int do_semop(int sems, int num, int op, int flg);

/* Set the flag to ignore a counter semaphore */
#define SEM_COUNTER_OFFLINE(STYPE, TSD, CSA, REG)										\
{																\
	if ((NULL != TSD) && (TSD)->mumps_can_bypass)										\
	{															\
		(TSD)->STYPE##_counter_halted = TRUE;										\
		send_msg_csa(CSA_ARG(CSA) VARLSTCNT(7) ERR_NOMORESEMCNT, 5, LEN_AND_LIT(#STYPE), FILE_TYPE_DB, DB_LEN_STR(REG));\
	}															\
}


/* Check whether a counter semaphore is ignored or not */
#define IS_SEM_COUNTER_ONLINE(HDR, COUNTER_HALTED) (!((HDR)->mumps_can_bypass) || !(COUNTER_HALTED))

#endif /* DO_SEMOP_INCLUDED */
