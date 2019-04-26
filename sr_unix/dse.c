/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> a6cd7b01f... GT.M V6.3-008
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
#include <stdarg.h>

#include "gtm_inet.h"
#include "gtm_signal.h"

#include "mlkdef.h"
#include "gtm_stdlib.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "stp_parms.h"
#include "error.h"
#include "interlock.h"
#include "gtmimagename.h"
#include "stringpool.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdskill.h"
#include "gdscc.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "tp.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "util.h"
>>>>>>> a6cd7b01f... GT.M V6.3-008
#include "cli.h"

int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "dse_main");
}
