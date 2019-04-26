/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

<<<<<<< HEAD
=======
#include "gtm_limits.h"
#include "gtm_inet.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_time.h"
#include "gtm_stat.h"
#include <stddef.h>
#include <errno.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "mlkdef.h"
#include "gdscc.h"
#include "cmidef.h"
#include "hashtab_mname.h"
#include "cmmdef.h"
#include "cmi.h"
#include "gt_timer.h"
#include "gtcmlkdef.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmimagename.h"
#include "trans_log_name.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdskill.h"
#include "buddy_list.h"
#include "tp.h"
>>>>>>> a6cd7b01f... GT.M V6.3-008
#include "cli.h"

int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "gtcm_gnp_server_main");
}
