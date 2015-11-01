/****************************************************************
 *								*
 *	Copyright 2001, 2005 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "min_max.h"

/* Note: GT.M code *MUST*NOT* make use of the sleep() function because use of the sleep() function
   causes problems with GT.M's timers on some platforms. Specifically, the sleep() function
   causes the SIGARLM handler to be silently deleted on Solaris systems (through Solaris 9 at least).
   This leads to lost timer pops and has the potential for system hangs. The proper long sleep mechanism
   is hiber_start which can be accessed through the LONG_SLEEP macro defined in mdef.h.
 */

#define MAXSLPTIME 		100			/* max (millisec) sleep possible thru wcs_sleep */

/* 650 ==> incremental count to make a complete 1 min sleep */

#define MAXWTSTARTWAIT 		650
#define BUF_OWNER_STUCK 	650
#define UNIX_GETSPACEWAIT	(BUF_OWNER_STUCK * 2)
#define MAXGETSPACEWAIT 	650
#define MAX_CRIT_TRY		650
#define MAX_BACKUP_FLUSH_TRY	650
#define MAX_OPEN_RETRY		650		/* vms only: for dbfilop  and others trying to open the db file */
#define MAX_SHMGET_COUNT	650		/* unix only: 1 min try to get shared memory */
#define JNL_MAX_FLUSH_TRIES     650
#define JNL_FLUSH_PROG_FACTOR	2
#define JNL_FLUSH_PROG_TRIES	(JNL_MAX_FLUSH_TRIES * JNL_FLUSH_PROG_FACTOR)
#define MAX_LCK_TRIES 		650		/* vms only: wait in mu_rndwn_file */
#define TIME_TO_FLUSH           10      	/* milliseconds */
#define MAX_FSYNC_WAIT_CNT     	1150		/* 2 mins of total wait for fsync, before GTMASSERTing */

#define LOOP_CNT_SEND_WAKEUP	300		/* When loops hit multiple of this count, they can send
						   a wakeup (resume/continue) to the process */
#define	SLEEP_JNLQIOLOCKWAIT	1		/* 1-msec wait */
#define	MAXJNLQIOLOCKWAIT	4000		/* 4sec = 4000 1-msec waits to see if io_in_prog lock is free in wcs_flu */

#define	SLEEP_WRTLATCHWAIT	1		/* 1-msec wait */
#define	MAXWRTLATCHWAIT		1000		/* 1sec = 1000 * 1-msec time waits to see if write-latch value of a
						 * 	cache-record becomes free (i.e. LATCH_CLEAR) in db_csh_getn() */
#define RETRY_CASLATCH_CUTOFF	16		/* retry loop index cutoff to try performCASLatchCheck() */

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
