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

#include "mdef.h"

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
#include "iosp.h"
#include "repl_shutdcode.h"
#include "gtmrecv.h"
#include "repl_log.h"
#include "repl_dbg.h"

void gtmrecv_exit(int exit_status)
{
	error_def(ERR_REPLEXITERR);
#ifdef VMS
	sys$exit((0 == exit_status) ? SS$_NORMAL : ERR_REPLEXITERR);
#else
	exit(exit_status);
#endif
}
