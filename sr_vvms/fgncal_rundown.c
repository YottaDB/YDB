/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <psldef.h>
#include <descrip.h>
#include "gtm_inet.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "iotimer.h"
#include "repl_msg.h"	/* needed by gtmsource.h */
#include "gtmsource.h"	/* needed for jnlpool_addrs and etc. */
#include "repl_shm.h"	/* needed for DETACH_FROM_JNLPOOL macro */
#include "op.h"
#include <rtnhdr.h>
#include "lv_val.h"	/* needed for "fgncal.h" */
#include "fgncal.h"
#include "gv_rundown.h"
#include "filestruct.h"
#include "jnl.h"
#include "dpgbldir.h"

GBLREF	gv_key			*gv_currkey;
GBLREF	boolean_t	        pool_init;
GBLREF	jnlpool_addrs	        jnlpool;
GBLREF	jnlpool_ctl_ptr_t	jnlpool_ctl;
GBLREF	uint4			dollar_tlevel;

void fgncal_rundown(void)
{
	sys$cantim(0,0);	/* cancel all outstanding timers.  prevents unwelcome surprises */
	if (gv_currkey != NULL)
	{
		gv_currkey->end = 0;	/* key no longer valid, since all databases are closed */
		gv_currkey->base[0] = 0;
	}
	finish_active_jnl_qio();
	DETACH_FROM_JNLPOOL(pool_init, jnlpool, jnlpool_ctl);
	if (dollar_tlevel)
		OP_TROLLBACK(0);
	op_lkinit();
	op_unlock();
	op_zdeallocate(NO_M_TIMEOUT);
	gv_rundown();	/* run down all databases */
	gd_rundown();
}
