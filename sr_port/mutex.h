/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEX_H
#define MUTEX_H

#include "mutexsp.h"

#define MUTEX_HARD_SPIN_COUNT		128
#define MUTEX_SLEEP_SPIN_COUNT		128
#define MUTEX_SPIN_SLEEP_MASK		0	/* default to cause rel_quant */
<<<<<<< HEAD

#define MUTEXLCKALERT_INTERVAL		32	/* seconds [UNIX only] */
||||||| parent of 19e495f7cb (GT.M V7.1-003)
#define MUTEX_WRITE_HARD_SPIN_COUNT	MUTEX_HARD_SPIN_COUNT
#define MUTEX_WRITE_SLEEP_SPIN_COUNT	MUTEX_SLEEP_SPIN_COUNT
#define MUTEX_WRITE_SPIN_SLEEP_MASK	MUTEX_SPIN_SLEEP_MASK

#define MUTEX_MAX_OPTIMISTIC_ATTEMPTS 		1024

#define MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR	3
=======
#define MUTEX_WRITE_HARD_SPIN_COUNT	MUTEX_HARD_SPIN_COUNT
#define MUTEX_WRITE_SLEEP_SPIN_COUNT	MUTEX_SLEEP_SPIN_COUNT
#define MUTEX_WRITE_SPIN_SLEEP_MASK	MUTEX_SPIN_SLEEP_MASK
#define MUTEX_MAX_OPTIMISTIC_ATTEMPTS 		1024
#define MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR	3
#define MAX_MUTEX_CLNS (1 << 3) /* Must be an exact power of two. */

typedef struct mutex_cln_ctl_struct
{
	char	filler[12];
	uint4	top;
	uint4	pids[MAX_MUTEX_CLNS];
	seq_num	seqnos[MAX_MUTEX_CLNS];
} mutex_cln_ctl_struct;

enum mutex_cln_status
{
	mutex_cln_absent = 0,
	mutex_cln_present,
	mutex_cln_invalid
};

typedef struct mutex_cln_info
{
	uint4	pid;
	char	filler[4];
	seq_num	seqno;
} mutex_cln_info;

void rollback_mutex_cln_ctl(seq_num max_seqno);
>>>>>>> 19e495f7cb (GT.M V7.1-003)

#endif /* MUTEX_H */
