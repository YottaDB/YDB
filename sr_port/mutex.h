/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
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

#define MUTEXLCKALERT_INTERVAL		32	/* seconds [UNIX only] */

#endif /* MUTEX_H */
