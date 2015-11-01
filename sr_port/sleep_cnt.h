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

#define MAXSLPTIME 		100			/* max (millisec) sleep possible thru wcs_sleep */

/* 650 ==> incremental count to make a complete 1 min sleep */

#define MAXWTSTARTWAIT 		650
#define BUF_OWNER_STUCK 	650
#define MAXGETSPACEWAIT 	650
#define MAX_CRIT_TRY		650
#define MAX_BACKUP_FLUSH_TRY	650
#define MAX_OPEN_RETRY		650			/* vms only: for dbfilop  and others trying to open the db file */
#define MAX_SHMGET_COUNT	650			/* unix only: 1 min try to get shared memory */
#define JNL_MAX_FLUSH_TRIES     650
#define JNL_FLUSH_PROG_FACTOR	2
#define JNL_FLUSH_PROG_TRIES	(JNL_MAX_FLUSH_TRIES * JNL_FLUSH_PROG_FACTOR)
#define MAX_LCK_TRIES 		650			/* vms only: wait in mu_rndwn_file */
#define TIME_TO_FLUSH           10      		/* milliseconds */
#define MAX_FSYNC_WAIT_CNT     	1150			/* 2 mins of total wait for fsync, before GTMASSERTing */
#define	MAXJNLQIOLOCKWAIT	4000			/* 20sec = 4000 5-msec waits to see if io_in_prog lock is free in wcs_flu */
#define	PRC_STATE_CNT		(1000/MAXSLPTIME)	/* check process death state apprx. every 1 sec while trying to grab */
