/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FTOK_SEMS_INCLUDED
#define FTOK_SEMS_INCLUDED

#include "gtm_semutils.h"

#define	DECR_CNT_FALSE		FALSE
#define	DECR_CNT_TRUE		TRUE
#define	DECR_CNT_SAFE		2

boolean_t ftok_sem_get2(gd_region *reg, boolean_t *stacktrace_time, boolean_t *timedout, semwait_status_t *retstat,
			boolean_t *bypass, boolean_t *ftok_counter_halted, boolean_t incr_cnt);
boolean_t ftok_sem_get(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate,
		       boolean_t *ftok_counter_halted);
boolean_t ftok_sem_get_common(gd_region *reg, boolean_t incr_cnt, int project_id, boolean_t immediate, boolean_t *stacktrace_time,
				boolean_t *timedout, semwait_status_t *retstat, boolean_t *bypass, boolean_t *ftok_counter_halted);
boolean_t ftok_sem_lock(gd_region *reg, boolean_t immediate);
boolean_t ftok_sem_release(gd_region *reg, boolean_t decr_cnt, boolean_t immediate);

#define MAX_SEMGET_RETRIES		100

#endif /* FTOK_SEMS_INCLUDED */
