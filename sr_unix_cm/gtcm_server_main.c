/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2018 YottaDB LLC. and/or its subsidiaries.*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_server_main.c ---
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
#include "ydb_chk_dist.h"
#include "send_msg.h"
#include "wbox_test_init.h"
#include "cli.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF	int		rc_server_id;
GBLREF	char		image_id[256];
GBLREF	omi_conn_ll	*omi_conns;
GBLREF	int4		omi_errno;
GBLREF	int4		gtcm_stime;
GBLREF	int4		gtcm_ltime;
GBLREF	int4		gtcm_exi_condition;
GBLREF	int		omi_pid;
GBLREF	int		history;
GBLREF	int		authenticate;
GBLREF	int		ping_keepalive;
GBLREF	char		*omi_service;

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int gtcm_server_main(int argc, char_ptr_t argv[], char **envp)
{
	omi_conn_ll	conns;
	bool	  	set_pset();
	int 		ret_val;
	DCL_THREADGBL_ACCESS;

	GTM_THREADGBL_INIT;
	common_startup_init(GTCM_SERVER_IMAGE, NULL);	/* The GTCM server does not have any command tables
							 * so pass command array as NULL.
							 */
	SPRINTF(image_id,"%s=gtcm_server", image_id);
#	ifdef SEQUOIA
	if (!set_pset())
		EXIT(-1);
#	endif
	/*  Initialize everything but the network */
	err_init(gtcm_exit_ch);
	ydb_chk_dist(argv[0]);
	omi_errno = OMI_ER_NO_ERROR;
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
	SPRINTF(image_id,"%s(pid=%d) %s %s %s -id %d -service %s",
		image_id,
		omi_pid,
		( history ? "-hist" : "" ),
		( authenticate ? "-auth" : "" ),
		( ping_keepalive ? "-ping" : "" ),
		rc_server_id,
		omi_service
		);
	OPERATOR_LOG_MSG;
	omi_conns = &conns;
	/*  Should be forever, unless an error occurs */
	gtcm_loop(&conns);
	/*  Clean up */
	gtcm_end_net(&conns);
	gtcm_exit();
	return 0;
}
