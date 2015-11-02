/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_play.c ---
 *
 *	Main routine for the GTCM packet log replayer.
 *
 */

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_time.h"		/* for time() */
#include "gt_timer.h"		/* for set_blocksig() */
#include "gtm_fcntl.h"
#include "gtm_string.h"		/* for strerror() */
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "gtcm.h"
#include "error.h"
#include "gtm_env_init.h"	/* for gtm_env_init() prototype */
#include "gtm_threadgbl_init.h"
#include "gtmimagename.h"
#include "gtm_imagetype_init.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLDEF short		 gtcm_ast_avail;
GBLDEF int4		 gtcm_exi_condition;
GBLDEF char		*omi_service = (char *)0;
GBLDEF FILE		*omi_debug   = (FILE *)0;
GBLDEF char		*omi_pklog   = (char *)0;
GBLDEF char		*omi_pklog_addr = (char *)0;
GBLDEF int		 omi_pkdbg   = 0;
GBLDEF omi_conn_ll	*omi_conns   = (omi_conn_ll *)0;
GBLDEF int		 omi_exitp   = 0;
GBLDEF int		 omi_pid     = 0;
GBLDEF int4		 omi_errno   = 0;
GBLDEF int4		 omi_nxact   = 0;
GBLDEF int4		 omi_nxact2  = 0;
GBLDEF int4		 omi_nerrs   = 0;
GBLDEF int4		 omi_brecv   = 0;
GBLDEF int4		 omi_bsent   = 0;
GBLDEF int4		 gtcm_stime  = 0;  /* start time for GT.CM */
GBLDEF int4		 gtcm_ltime  = 0;  /* last time stats were gathered */
GBLDEF int		 one_conn_per_inaddr = 0;
GBLDEF int		 authenticate = 0;   /* authenticate OMI connections */
GBLDEF int 		 psock = -1;         /* pinging socket */
GBLDEF int		 ping_keepalive = 0; /* check connections using ping */
GBLDEF int		 conn_timeout = TIMEOUT_INTERVAL;
GBLDEF int		 history = 0;
#ifdef GTCM_RC
GBLREF int		 rc_nxact;
GBLREF int		 rc_nerrs;
#endif /* defined(GTCM_RC) */

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int main(int argc, char_ptr_t argv[])
{
	omi_conn	*cptr, conn;
	int		i;
	char		buff[OMI_BUFSIZ];
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	set_blocksig();
	gtm_imagetype_init(GTCM_SERVER_IMAGE);
	gtm_env_init(); /* read in all environment variables before calling any function particularly malloc (from err_init below)*/
	/*  Open the packet log file for playback */
	if (1 == argc)
		conn.fd = fileno(stdin);
	else if (2 == argc)
	{
		if (INV_FD_P((conn.fd = open(argv[argc - 1], O_RDONLY))))
		{
			PRINTF("%s: open(\"%s\"): %s\n", argv[0], argv[argc - 1],
			       STRERROR(errno));
			exit(-1);
		}
	} else
	{
		PRINTF("%s: bad command line arguments\n\t%s [ filename ]\n",
		       argv[0], argv[0]);
		exit(-1);
	}
	/*  Initialize everything but the network */
	err_init(gtcm_exit_ch);
	omi_errno = OMI_ER_NO_ERROR;
	ESTABLISH_RET(omi_dbms_ch, -1);	/* any return value to signify error return */
	gtcm_init(argc, argv);
#	ifdef GTCM_RC
	rc_create_cpt();
#	endif
	REVERT;
	if (omi_errno != OMI_ER_NO_ERROR)
		exit(omi_errno);
	/*  Initialize the connection structure */
	conn.next   = (omi_conn *)0;
	conn.bsiz   = OMI_BUFSIZ;
	conn.bptr   = conn.buff   = buff;
	conn.xptr   = (char *)0;
	conn.blen   = 0;
	conn.exts   = 0;
	conn.state  = OMI_ST_DISC;
	conn.ga     = (ga_struct *)0;	/* struct gd_addr_struct */
	conn.of     = (oof_struct *)0;	/* struct rc_oflow */
	conn.pklog  = FD_INVALID;
	/*  Initialize the statistics */
	conn.stats.bytes_recv = 0;
	conn.stats.bytes_send = 0;
	conn.stats.start      = time((time_t *)0);
	for (i = 0; i < OMI_OP_MAX; i++)
		conn.stats.xact[i] = 0;
	for (i = 0; i < OMI_ER_MAX; i++)
		conn.stats.errs[i] = 0;

	for (;;)
		if (omi_srvc_xact(&conn) < 0)
			break;
	PRINTF("%ld seconds connect time\n", time((time_t)0) - conn.stats.start);
	PRINTF("%d OMI transactions\n", omi_nxact);
	PRINTF("%d OMI errors\n", omi_nerrs);
#	ifdef GTCM_RC
	PRINTF("%d RC transactions\n", rc_nxact);
	PRINTF("%d RC errors\n", rc_nerrs);
#	endif /* defined(GTCM_RC) */
	PRINTF("%d bytes recv'd\n", conn.stats.bytes_recv);
	PRINTF("%d bytes sent\n", conn.stats.bytes_send);
	gtcm_exit();
	return 0;
}
