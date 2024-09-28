/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
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

#include <sys/sem.h>
#include "gtm_sem.h"

error_def(ERR_NOMORESEMCNT);

int do_semop(int sems, int num, int op, int flg);
boolean_t is_sem_undo_overflow(int semid, struct sembuf *sops, int sopcnt);

/* Check whether a counter semaphore is ignored or not */
#define IS_SEM_COUNTER_ONLINE(HDR, COUNTER_HALTED) (!((HDR)->mumps_can_bypass) || !(COUNTER_HALTED))

#endif /* DO_SEMOP_INCLUDED */
