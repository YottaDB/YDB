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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <arpa/inet.h>
#ifdef UNIX
#include <sys/ipc.h>
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
#include "gtmrecv.h"
#include "error.h"
#include "dpgbldir.h"

#ifdef REPL_RECVR_HELP_UPD
GBLREF	gd_addr                 *gd_header;
#endif

CONDITION_HANDLER(gtmrecv_ch)
{
#ifdef REPL_RECVR_HELP_UPD
	gd_addr		*addr_ptr;
	gd_region	*reg_local, *reg_top;
	sgmnt_addrs	*csa;
#endif

	error_def(ERR_ASSERT);
	error_def(ERR_CTRLC);
	error_def(ERR_FORCEDHALT);
	error_def(ERR_GTMCHECK);
	error_def(ERR_GTMASSERT);
	error_def(ERR_STACKOFLOW);
	error_def(ERR_OUTOFSPACE);

	START_CH;
	if (!(IS_GTM_ERROR(SIGNAL)) || DUMPABLE || SEVERITY == ERROR)
	{
#ifdef REPL_RECVR_HELP_UPD
		for (addr_ptr = get_next_gdr(0); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
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
#endif
       		NEXTCH;
	}
	/* warning, info, or success */
	CONTINUE;
}
