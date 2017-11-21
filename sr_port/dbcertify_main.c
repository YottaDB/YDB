/****************************************************************
 *								*
 * Copyright 2005, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2017 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/****************************************************************
        DBCERTIFY - main driver

        Parse arguments, invoke required phase routine.

        Note: Most routines in this utility are self-contained
              meaning they do not reference GT.M library routines
              (with some notable exceptions). This is because
              phase-1 is going to run against live V4 databases
              and the V5 compilation will be using V5 database
              structures.
 ****************************************************************/

#include "mdef.h"

#include "gtm_ctype.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_unistd.h"
#include "gtm_stdlib.h"

#include <signal.h>

#include "continue_handler.h"
#include "sig_init.h"
#include "gdsroot.h"
#include "v15_gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "v15_gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "v15_gdsfhead.h"
#include "gdsblkops.h"
#include "mupip_exit.h"
#include "gtmimagename.h"
#include "error.h"
#include "iosp.h"
#include "dbcertify.h"
#include "cli.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "wbox_test_init.h"

GBLREF	uint4			process_id;
GBLREF	boolean_t		gtm_utf8_mode;
GBLREF	CLI_ENTRY		dbcertify_cmd_ary[];

GBLDEF	phase_static_area	*psa_gbl;			/* Global anchor for static area */

int dbcertify_main(int argc, char **argv, char **envp)
{
	DCL_THREADGBL_ACCESS;

	/* Initialization of scaffolding we run on */
	GTM_THREADGBL_INIT;
	common_startup_init(DBCERTIFY_IMAGE, &dbcertify_cmd_ary[0]);
	gtm_utf8_mode = FALSE; 		/* Only ever runs in V4 database so NO utf8 mode -- ever */
	psa_gbl = malloc(SIZEOF(*psa_gbl));
	memset(psa_gbl, 0, SIZEOF(*psa_gbl));
	err_init(dbcertify_base_ch);
	sig_init(dbcertify_signal_handler, dbcertify_signal_handler, NULL, continue_handler);
	/* Structure checks .. */
	assert((24 * 1024) == SIZEOF(v15_sgmnt_data));	/* Verify V4 file header hasn't suddenly increased for some odd reason */
	OPERATOR_LOG_MSG;
	/* Platform dependent method to get the option scan going and invoke necessary driver routine */
	dbcertify_parse_and_dispatch(argc, argv);
	return SS_NORMAL;
}
