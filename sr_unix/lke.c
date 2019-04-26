/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"
<<<<<<< HEAD
=======
#include "stringpool.h"
#include "interlock.h"
#include "gtmimagename.h"
#include "gdsroot.h"
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
#include "gt_timer.h"
#include "lke.h"
#include "lke_fileio.h"
#include "gtm_startup_chk.h"
#include "generic_signal_handler.h"
#include "cli_parse.h"
#include "getzdir.h"
#include "getjobname.h"
#include "sig_init.h"
#include "gtmmsg.h"
#include "suspsigs_handler.h"
#include "patcode.h"
#include "common_startup_init.h"
#include "gtm_threadgbl_init.h"
#include "gtmio.h"
#include "have_crit.h"
#include "gt_timers_add_safe_hndlrs.h"
#include "continue_handler.h"

#ifdef UTF8_SUPPORTED
# include "gtm_icu_api.h"
# include "gtm_utf8.h"
# include "gtm_conv.h"
GBLREF	u_casemap_t 		gtm_strToTitle_ptr;		/* Function pointer for gtm_strToTitle */
#endif

GBLREF VSIG_ATOMIC_T		util_interrupt;
GBLREF bool			licensed;
GBLREF void			(*func)(void);
GBLREF spdesc			rts_stringpool, stringpool;
GBLREF global_latch_t		defer_latch;
GBLREF char			cli_err_str[];
GBLREF CLI_ENTRY		lke_cmd_ary[];
GBLREF ch_ret_type		(*stpgc_ch)();			/* Function pointer to stp_gcol_ch */

GBLDEF CLI_ENTRY		*cmd_ary = &lke_cmd_ary[0];	/* Define cmd_ary to be the LKE specific cmd table */
>>>>>>> a6cd7b01f... GT.M V6.3-008

int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "lke_main");
}
