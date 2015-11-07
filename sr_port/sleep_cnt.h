/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef SLEEP_CNT_H_INCLUDED
#define SLEEP_CNT_H_INCLUDED

#include "min_max.h"

/* Note: GT.M code *MUST*NOT* make use of the sleep() function because use of the sleep() function	(BYPASSOK - sleep())
   causes problems with GT.M's timers on some platforms. Specifically, the sleep() function
   causes the SIGARLM handler to be silently deleted on Solaris systems (through Solaris 9 at least).
   This leads to lost timer pops and has the potential for system hangs. The proper long sleep mechanism
   is hiber_start which can be accessed through the LONG_SLEEP macro defined in mdef.h.
 */

/* It has been found that on some platforms wcs_sleep(1msec) takes a lot longer than 1 msec to return. We think
 * this is because the frequency of the kernel's timer interrupt is a lot lower in older kernels at least in Linux.
 * Therefore we should never use MINSLPTIME (1msec) in sleep loops as it will result in a total loop sleep time
 * that is potentially an order of magnitude higher than the desired total sleep time. 10msec seems to work a lot
 * more effectively so use that for now until all unix kernels can support 1msec sleep granularity more accurately.
 */
#define MINSLPTIME 		1	/* min (millisec) sleep possible thru wcs_sleep. See comment above about loop usage */
#define MAXSLPTIME 		10	/* max (millisec) sleep possible thru wcs_sleep */
#define	SLEEP_ONE_MIN		6000	/* # of wcs_sleep iterations (each max MAXSLPTIME msec) needed to wait 1 minute */

#define MAXWTSTARTWAIT 		SLEEP_ONE_MIN
#define BUF_OWNER_STUCK 	SLEEP_ONE_MIN
#define UNIX_GETSPACEWAIT	(BUF_OWNER_STUCK * 2)
#define MAXGETSPACEWAIT 	SLEEP_ONE_MIN
#define MAX_CRIT_TRY		SLEEP_ONE_MIN
#define MAX_BACKUP_FLUSH_TRY	650
#define MAX_OPEN_RETRY		SLEEP_ONE_MIN	/* vms only: for dbfilop  and others trying to open the db file */
#define JNL_MAX_FLUSH_TRIES     SLEEP_ONE_MIN
#define JNL_FLUSH_PROG_FACTOR	2
#define JNL_FLUSH_PROG_TRIES	(JNL_MAX_FLUSH_TRIES * JNL_FLUSH_PROG_FACTOR)
#define MAX_LCK_TRIES 		SLEEP_ONE_MIN	/* vms only: wait in mu_rndwn_file */
#define FSYNC_WAIT_TIME     	(2 * SLEEP_ONE_MIN)	/* 2 mins of wait for fsync between JNLFSYNCSTUCK complaints */
#define FSYNC_WAIT_HALF_TIME    SLEEP_ONE_MIN	/* 1 min of wait for fsync between DEBUG JNLFSYNCSTUCK complaints */

#define PHASE2_COMMIT_SLEEP	MAXSLPTIME	/* 10 msec inter-iteration sleep wait for active phase2 commits */
#define	PHASE2_COMMIT_WAIT	SLEEP_ONE_MIN
#define	PHASE2_COMMIT_WAIT_HTBT	8		/* = 8 heartbeats (each 8 seconds) = 64 seconds wait (used in Unix) */

#define	SLEEP_INSTFREEZEWAIT	100		/* 100-msec wait between re-checks of instance freeze status */
#define	SLEEP_IORETRYWAIT	500		/* 500-msec wait between retries of the same write operation */

#define	SLEEP_JNLQIOLOCKWAIT	1		/* 1-msec wait */
#define	MAXJNLQIOLOCKWAIT	4000		/* 4sec = 4000 1-msec waits to see if io_in_prog lock is free in wcs_flu */

#define	SLEEP_WRTLATCHWAIT	1		/* 1-msec wait */
#define	MAXWRTLATCHWAIT		1000		/* 1sec = 1000 * 1-msec time waits to see if write-latch value of a
						 * 	cache-record becomes free (i.e. LATCH_CLEAR) in db_csh_getn() */
#define RETRY_CASLATCH_CUTOFF	16		/* retry loop index cutoff to try performCASLatchCheck() */
#define MAXWAIT2KILL		(2 * SLEEP_ONE_MIN) /* KILLs wait for MAXWAIT2KILL minute(s) for inhibit_kills
						     * to become zero */

/*  For use by spin locks, SLEEP is ms, total should be under a minute */
#define LOCK_TRIES		(50 * 4 * 1000) /* outer loop: 50 secs, 1 loop in 4 is sleep of 1 ms */
#define LOCK_SPINS		1024		/* inner spin loop base */
#define LOCK_SPINS_PER_4PROC	256		/* Additional lock spins for every 4 processors past first 8 */
#define LOCK_SLEEP		1		/* very short sleep before repoll lock */

/* To compute the maximum duration of an inner spinloop, the following macro can be
   used. The theory behind this macro is that the basic definition of LOCK_SPINS is
   good for approximately 8 processors but needs to be appropriately increased for
   each additional 4 processors.
*/
#define MAX_LOCK_SPINS(base, proc) (base + MAX(0, ((((proc - 7) * LOCK_SPINS_PER_4PROC) / 4))))

/* Maximum duration (in minutes) that a process waits for the completion of read-in-progress after which
 * it stops waiting but rather continue fixing the remaining cache records. This is done to avoid
 * waiting a long time in case there are many corrupt global buffers. After waiting 1 minute each for the
 * first 4 cache-records (a wait time of 4 mins in total), we might as well stop waiting more and fix
 * the remaining crs. The value of 4 minutes was chosen because that is what t_qread currently has as its
 * maximum wait for reading a block from disk into the global buffer. */
#define MAX_WAIT_FOR_RIP	4

#endif
