/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_main.c ---
 *
 *	Main routine for the GTCM server.  Initialize everything and
 *	then run forever.
 *
 */

#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_stdlib.h"		/* for EXIT() */
#include "gtm_time.h"		/* for time() */
#include "gtm_signal.h"

#include <sys/types.h>
#include <errno.h>

#include "gtcm.h"
#include "error.h"
#include "gtm_threadgbl_init.h"
#include "gtmimagename.h"
#include "common_startup_init.h"
#include "gtm_startup_chk.h"
#include "send_msg.h"
#include "wbox_test_init.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

#define	MAX_IMAGE_NAME_LEN	256 + 1

GBLDEF short		gtcm_ast_avail;
GBLDEF int4		gtcm_exi_condition;

/* image_id....allows you to determine info about the server
 * by using the strings command, or running dbx
 */
GBLDEF char		image_id[MAX_IMAGE_NAME_LEN];
GBLDEF char		*omi_service = (char *)0;
GBLDEF FILE		*omi_debug   = (FILE *)0;
GBLDEF char		*omi_pklog   = (char *)0;
GBLDEF char		*omi_pklog_addr = (char *)0;
GBLDEF int		 omi_pkdbg   = 0;
GBLDEF omi_conn_ll	*omi_conns   = (omi_conn_ll *)0;
GBLDEF int		 omi_exitp   = 0;
GBLDEF int		 omi_pid     = 0;
GBLDEF int4		 omi_errno   = 0;
GBLDEF int4		 omi_nxact   = 0;	/* # of transactions */
GBLDEF int4		 omi_nxact2  = 0;	/* transactions since last stat dump */
GBLDEF int4		 omi_nerrs   = 0;
GBLDEF int4		 omi_brecv   = 0;
GBLDEF int4		 omi_bsent   = 0;
GBLDEF int4		 gtcm_stime  = 0;	/* start time for GT.CM */
GBLDEF int4		 gtcm_ltime  = 0;	/* last time stats were dumped */
GBLDEF int		 one_conn_per_inaddr = -1;
GBLDEF int		 authenticate = 0;	/* authenticate OMI connections */
GBLDEF int		 psock = -1;		/* pinging socket */
GBLDEF int		 ping_keepalive = 0;	/* check connections using ping */
GBLDEF int		 conn_timeout = TIMEOUT_INTERVAL;
GBLDEF int		 history = 0;
GBLREF int	 	 rc_server_id;

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int main(int argc, char_ptr_t argv[])
{
	omi_conn_ll	conns;
	bool	  	set_pset();
	int 		ret_val;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	ctxt = NULL;
	common_startup_init(GTCM_SERVER_IMAGE);
	SNPRINTF(image_id, MAX_IMAGE_NAME_LEN, "%s=gtcm_server", "image_id");
#	ifdef SEQUOIA
	if (!set_pset())
		EXIT(-1);
#	endif
	/*  Initialize everything but the network */
	err_init(gtcm_exit_ch);
	gtm_chk_dist(argv[0]);
	omi_errno = OMI_ER_NO_ERROR;
	ctxt = ctxt;
	ESTABLISH_RET(omi_dbms_ch, -1);	/* any return value to signify error return */
	gtcm_init(argc, argv);
	gtcm_ltime = gtcm_stime = (int4)time(0);
#	ifdef GTCM_RC
	rc_create_cpt();
#	endif
	REVERT;
	if (OMI_ER_NO_ERROR != omi_errno)
		EXIT(omi_errno);
	/*  Initialize the network interface */
	if (0 != (ret_val = gtcm_bgn_net(&conns)))	/* Warning - assignment */
	{
		gtcm_rep_err("Error initializing TCP", ret_val);
		gtcm_exi_condition = ret_val;
		gtcm_exit();
	}
	SNPRINTF(image_id, MAX_IMAGE_NAME_LEN, "image_id=gtcm_server(pid=%d) %s %s %s -id %d -service %s",
		omi_pid,
		( history ? "-hist" : "" ),
		( authenticate ? "-auth" : "" ),
		( ping_keepalive ? "-ping" : "" ),
		rc_server_id,
		omi_service
		);
	OPERATOR_LOG_MSG;
	omi_conns = &conns;
	INIT_FNPTR_GLOBAL_VARIABLES;
	/*  Should be forever, unless an error occurs */
	gtcm_loop(&conns);
	/*  Clean up */
	gtcm_end_net(&conns);
	gtcm_exit();
	return 0;
}
