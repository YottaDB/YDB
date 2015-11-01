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

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>

#include <errno.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "hashdef.h"
#include "ast.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "error.h"
#ifdef UNIX
#include "io.h"
#include "gtmsecshr.h"
#include "mutex.h"
#endif
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "gvcmy_rundown.h"
#include "rc_cpt_ops.h"
#include "gv_rundown.h"

GBLREF	bool			update_trans;
GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	boolean_t		pool_init;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	jnlpool_ctl_ptr_t      	jnlpool_ctl;

void gv_rundown(void)
{
	gd_region	*r_top, *r_save, *r_local;
	gd_addr		*addr_ptr;
	error_def(ERR_TEXT);

	r_save = gv_cur_region;		/* Save for possible core dump */
	gvcmy_rundown();
	update_trans = TRUE;
	ENABLE_AST

	if (TRUE == pool_init)
		rel_lock(jnlpool.jnlpool_dummy_reg);

	for (addr_ptr = get_next_gdr(0); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{	for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top;
				r_local++)
		{
			if (r_local->open && !r_local->was_open)
			{
				gv_cur_region = r_local;
			        tp_change_reg();
				gds_rundown();
			}
			r_local->open = r_local->was_open = FALSE;
		}
	}
	rc_close_section();
	gv_cur_region = r_save;		/* Restore value for dumps but this region is now closed and is otherwise defunct */

#ifdef UNIX
	gtmsecshr_sock_cleanup(CLIENT);
#ifndef MUTEX_MSEM_WAKE
	mutex_sock_cleanup();
#endif
#endif
	jnlpool_detach();
}
