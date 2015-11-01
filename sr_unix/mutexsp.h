/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MUTEXSP_H
#define MUTEXSP_H

#define MUTEX_HARD_SPIN_COUNT		128
#define MUTEX_SLEEP_SPIN_COUNT		128
#define MUTEX_SHORT_WAIT_MS		2 /* ms.  Keep this a power of 2 */
#define MUTEX_SHORT_WAIT_US		(MUTEX_SHORT_WAIT_MS << 10) /* micro sec */
#define MUTEX_SPIN_SLEEP_MASK		(MUTEX_SHORT_WAIT_US - 1)

#define MUTEX_WRITE_HARD_SPIN_COUNT	MUTEX_HARD_SPIN_COUNT
#define MUTEX_WRITE_SLEEP_SPIN_COUNT	MUTEX_SLEEP_SPIN_COUNT
#define MUTEX_WRITE_SPIN_SLEEP_MASK	MUTEX_SPIN_SLEEP_MASK

#define MUTEX_MAX_OPTIMISTIC_ATTEMPTS 		1024

#define MUTEX_MAX_WAIT_FOR_PROGRESS_CNTR	3

#define MUTEX_MAX_WRITE_LOCK_ATTEMPTS		8

#define MICROSEC_SLEEP(x)		m_usleep(x)

#endif /* MUTEXSP_H */
