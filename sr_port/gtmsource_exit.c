/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdio.h"		/* for FILE * in repl_comm.h */
#include "gtm_stdlib.h"		/* for exit() */

#ifdef VMS
#include <ssdef.h>
#include <descrip.h>
#endif

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

void gtmsource_exit(int exit_status)
{
	error_def(ERR_REPLEXITERR);
#ifdef VMS
	sys$exit((0 == exit_status) ? SS$_NORMAL : ERR_REPLEXITERR);
#else
	exit(exit_status);
#endif
}
