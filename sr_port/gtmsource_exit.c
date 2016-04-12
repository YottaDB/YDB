/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"		/* for FILE * in repl_comm.h */
#include "gtm_stdlib.h"		/* for EXIT() */

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "repl_comm.h"

GBLREF	boolean_t		is_src_server;
GBLREF	gtmsource_options_t	gtmsource_options;

#ifdef VMS
error_def(ERR_REPLEXITERR);
#endif
error_def(ERR_REPLSRCEXITERR);

void gtmsource_exit(int exit_status)
{
	if ((0 != exit_status) && is_src_server)
		send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLSRCEXITERR, 2, gtmsource_options.secondary_instname,
				gtmsource_options.log_file);
	EXIT(exit_status);
}
