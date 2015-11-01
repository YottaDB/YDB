/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#define MAXSLPTIME 		100			/* max (millisec) sleep possible thru wcs_sleep */

/* 650 ==> incremental count to make a complete 1 min sleep */

#define MAXWTSTARTWAIT 		650
#define BUF_OWNER_STUCK 	650
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
