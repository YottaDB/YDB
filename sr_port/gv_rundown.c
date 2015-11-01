/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <netinet/in.h>	/* Required for gtmsource.h */

#include <errno.h>
#ifdef VMS
#include <descrip.h>	/* Required for gtmsource.h */
#include <nam.h>	/* Required for the nam$l_esa members */
#endif

#include "gtm_inet.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "gdskill.h"
#include "gdscc.h"
#include "min_max.h"		/* needed for gdsblkops.h */
#include "gdsblkops.h"
#include "filestruct.h"
#include "jnl.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
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
	sgm_info	*si;
#ifdef VMS
	vms_gds_info	*gds_info;
#endif

	error_def(ERR_TEXT);

	r_save = gv_cur_region;		/* Save for possible core dump */
	gvcmy_rundown();
	update_trans = TRUE;
	ENABLE_AST

	if (TRUE == pool_init)
		rel_lock(jnlpool.jnlpool_dummy_reg);

	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
		{
			if (r_local->open && !r_local->was_open && dba_cm != r_local->dyn.addr->acc_meth)
			{	/* Rundown has already occurred for GT.CM client regions through gvcmy_rundown() above.
			 	 * Hence the (dba_cm != ...) check in the if above. Note that for GT.CM client regions,
				 * region->open is TRUE although cs_addrs is NULL.
			 	 */
				gv_cur_region = r_local;
			        tp_change_reg();
				gds_rundown();
				/* Now that gds_rundown is done, free up the memory associated with the region.
				 * Ideally the following memory freeing code should go to gds_rundown, but
				 * GT.CM calls gds_rundown() and we want to reuse memory for GT.CM.
				 */
				if (NULL != cs_addrs)
				{
					if (cs_addrs->dir_tree)
					{
						free(cs_addrs->dir_tree->alt_hist);
						free(cs_addrs->dir_tree);
					}
					if (cs_addrs->sgm_info_ptr)
					{
						si = cs_addrs->sgm_info_ptr;
						assert(si->gv_cur_region == gv_cur_region);
						if (si->jnl_tail)
						{
							FREEUP_BUDDY_LIST(si->format_buff_list);
							FREEUP_BUDDY_LIST(si->jnl_list);
						}
						FREEUP_BUDDY_LIST(si->recompute_list);
						FREEUP_BUDDY_LIST(si->new_buff_list);
						FREEUP_BUDDY_LIST(si->tlvl_info_list);
						FREEUP_BUDDY_LIST(si->tlvl_cw_set_list);
						FREEUP_BUDDY_LIST(si->cw_set_list);
						free_hashtab(&si->blks_in_use);
						if (si->cr_array_size)
						{
							assert(NULL != si->cr_array);
							free(si->cr_array);
						}
						if (si->first_tp_hist)
							free(si->first_tp_hist);
						free(si);
					}
					if (cs_addrs->jnl)
					{
						assert(cs_addrs->jnl->region == gv_cur_region);
						if (cs_addrs->jnl->jnllsb)
						{
							UNIX_ONLY(assert(FALSE));
							free(cs_addrs->jnl->jnllsb);
						}
						free(cs_addrs->jnl);
					}
				}
				assert(gv_cur_region->dyn.addr->file_cntl->file_info);
				VMS_ONLY(
					gds_info = (vms_gds_info *)gv_cur_region->dyn.addr->file_cntl->file_info;
					if (gds_info->xabpro)
						free(gds_info->xabpro);
					if (gds_info->xabfhc)
						free(gds_info->xabfhc);
					if (gds_info->nam)
					{
						free(gds_info->nam->nam$l_esa);
						free(gds_info->nam);
					}
					if (gds_info->fab)
						free(gds_info->fab);
				)
				free(gv_cur_region->dyn.addr->file_cntl->file_info);
				free(gv_cur_region->dyn.addr->file_cntl);
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
