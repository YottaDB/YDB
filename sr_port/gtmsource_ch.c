/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

#include "gtm_unistd.h"
#include "gtm_fcntl.h"
#include "gtm_inet.h"

#ifdef UNIX
#include "gtm_ipc.h"
#include <sys/sem.h>
#elif defined(VMS)
#include <ssdef.h>
#include <descrip.h> /* Required for gtmrecv.h */
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
#include "repl_msg.h"
#include "repl_comm.h"
#include "gtmsource.h"
#include "error.h"
#include "dpgbldir.h"
#include "wbox_test_init.h"

#ifdef UNIX
#include "ftok_sems.h"
#endif

GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	boolean_t		is_src_server;
GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	int			gtmsource_sock_fd;

error_def(ERR_ASSERT);
error_def(ERR_CTRLC);
error_def(ERR_FORCEDHALT);
error_def(ERR_GTMASSERT);
error_def(ERR_GTMASSERT2);
error_def(ERR_GTMCHECK);
error_def(ERR_OUTOFSPACE);
error_def(ERR_REPLSRCEXITERR);
error_def(ERR_STACKOFLOW);
error_def(ERR_MEMORY);

CONDITION_HANDLER(gtmsource_ch)
{
	boolean_t	skip_repl_close;
	gd_addr		*addr_ptr;
	gd_region	*reg_local, *reg_top;
	sgmnt_addrs	*csa;

	START_CH(TRUE);
	if (!(IS_YDB_ERROR(SIGNAL)) || DUMPABLE || SEVERITY == ERROR)
	{
		for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
		{
			for (reg_local = addr_ptr->regions, reg_top = reg_local + addr_ptr->n_regions;
				reg_local < reg_top; reg_local++)
			{
				if (reg_local->open && !reg_local->was_open)
				{
					csa = (sgmnt_addrs *)&FILE_INFO(reg_local)->s_addrs;
					if (csa && (csa->now_crit))
						rel_crit(reg_local);
				}
			}
		}
		if ((NULL != jnlpool) && (jnlpool->jnlpool_ctl))
		{
			csa = (sgmnt_addrs *)&FILE_INFO(jnlpool->jnlpool_dummy_reg)->s_addrs;
			if (csa && csa->now_crit)
				rel_lock(jnlpool->jnlpool_dummy_reg);
		}
		if (is_src_server)
			send_msg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLSRCEXITERR, 2, gtmsource_options.secondary_instname,
					gtmsource_options.log_file);
		/* When WBTEST_INDUCE_TLSIOERR, intentionally skip the graceful close of the socket descriptor to
		 * induce a TLSIOERR in the Receiver Server */
#		ifdef DEBUG
		skip_repl_close = WBTEST_ENABLED(WBTEST_INDUCE_TLSIOERR);
#		else
		skip_repl_close = FALSE;
#		endif
		if (!skip_repl_close && (FD_INVALID != gtmsource_sock_fd)) /* Close the socket if open */
			repl_close(&gtmsource_sock_fd);
       		NEXTCH;
	}
	VMS_ONLY (
		/* warning, info, or success */
		CONTINUE;
	)
	assertpro(FALSE);
}
