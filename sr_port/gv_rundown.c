/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#ifdef VMS
#include <descrip.h>	/* Required for gtmsource.h */
#include <nam.h>	/* Required for the nam$l_esa members */
#endif

#include "gtm_inet.h"	/* Required for gtmsource.h */
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
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "ast.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "error.h"
#ifdef UNIX
#include "io.h"
#include "gtmsecshr.h"
#include "mutex.h"
#include "ftok_sems.h"
#endif

#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "gvcmy_rundown.h"
#include "rc_cpt_ops.h"
#include "gv_rundown.h"
#include "targ_alloc.h"
#ifdef GTM_CRYPT
#include "gtmcrypt.h"
#endif
#if defined(DEBUG) && defined(UNIX)
#include "anticipatory_freeze.h"
#endif

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	boolean_t		pool_init;
GBLREF	jnlpool_addrs		jnlpool;
GBLREF	recvpool_addrs		recvpool;
GBLREF	jnlpool_ctl_ptr_t      	jnlpool_ctl;
GBLREF	gd_region		*ftok_sem_reg;

#if defined(DEBUG) && defined(UNIX)
GBLREF	boolean_t		is_jnlpool_creator;
error_def(ERR_TEXT);
#endif
error_def(ERR_NOTALLDBRNDWN);

void gv_rundown(void)
{
	gd_region	*r_top, *r_save, *r_local;
	gd_addr		*addr_ptr;
	sgm_info	*si;
	int4		rundown_status = EXIT_NRM;			/* if gds_rundown went smoothly */
#	ifdef VMS
	vms_gds_info	*gds_info;
#	elif UNIX
	unix_db_info	*udi;
#	endif
#if defined(DEBUG) && defined(UNIX)
	sgmnt_addrs		*csa;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;

	r_save = gv_cur_region;		/* Save for possible core dump */
	gvcmy_rundown();
	ENABLE_AST

	if (pool_init)
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
#				if defined(DEBUG) && defined(UNIX)
				if (is_jnlpool_creator && ANTICIPATORY_FREEZE_AVAILABLE && TREF(gtm_test_fake_enospc))
				{	/* Clear ENOSPC faking now that we are running down */
					csa = REG2CSA(r_local);
					if (csa->nl->fake_db_enospc || csa->nl->fake_jnl_enospc)
					{
						send_msg_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_TEXT, 2, DB_LEN_STR(r_local), ERR_TEXT,
							     2, LEN_AND_LIT("Resetting fake_db_enospc and fake_jnl_enospc"));
						csa->nl->fake_db_enospc = FALSE;
						csa->nl->fake_jnl_enospc = FALSE;
					}
				}
#				endif
				gv_cur_region = r_local;
			        tp_change_reg();
				UNIX_ONLY(rundown_status |=) gds_rundown();

				/* Now that gds_rundown is done, free up the memory associated with the region.
				 * Ideally the following memory freeing code should go to gds_rundown, but
				 * GT.CM calls gds_rundown() and we want to reuse memory for GT.CM.
				 */
				if (NULL != cs_addrs)
				{
					if (NULL != cs_addrs->dir_tree)
						FREE_CSA_DIR_TREE(cs_addrs);
					if (cs_addrs->sgm_info_ptr)
					{
						si = cs_addrs->sgm_info_ptr;
						/* It is possible we got interrupted before initializing all fields of "si"
						 * completely so account for NULL values while freeing/releasing those fields.
						 */
						assert((si->tp_csa == cs_addrs) || (NULL == si->tp_csa));
						if (si->jnl_tail)
						{
							CAREFUL_FREEUP_BUDDY_LIST(si->format_buff_list);
							CAREFUL_FREEUP_BUDDY_LIST(si->jnl_list);
						}
						CAREFUL_FREEUP_BUDDY_LIST(si->recompute_list);
						CAREFUL_FREEUP_BUDDY_LIST(si->new_buff_list);
						CAREFUL_FREEUP_BUDDY_LIST(si->tlvl_info_list);
						CAREFUL_FREEUP_BUDDY_LIST(si->tlvl_cw_set_list);
						CAREFUL_FREEUP_BUDDY_LIST(si->cw_set_list);
						if (NULL != si->blks_in_use)
						{
							free_hashtab_int4(si->blks_in_use);
							free(si->blks_in_use);
							si->blks_in_use = NULL;
						}
						if (si->cr_array_size)
						{
							assert(NULL != si->cr_array);
							if (NULL != si->cr_array)
								free(si->cr_array);
						}
						if (NULL != si->first_tp_hist)
							free(si->first_tp_hist);
						free(si);
					}
					if (cs_addrs->jnl)
					{
						assert(&FILE_INFO(cs_addrs->jnl->region)->s_addrs == cs_addrs);
						if (cs_addrs->jnl->jnllsb)
						{
							UNIX_ONLY(assert(FALSE));
							free(cs_addrs->jnl->jnllsb);
						}
						free(cs_addrs->jnl);
					}
					GTMCRYPT_ONLY(
						if (cs_addrs->encrypted_blk_contents)
							free(cs_addrs->encrypted_blk_contents);
					)
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
	cs_addrs = NULL;
	GTMCRYPT_ONLY(GTMCRYPT_CLOSE;)
#ifdef UNIX
	gtmsecshr_sock_cleanup(CLIENT);
#ifndef MUTEX_MSEM_WAKE
	mutex_sock_cleanup();
#endif
#endif
	jnlpool_detach();
#	ifdef UNIX
	/* Clean up any left-over ftok semaphores. This part of the code can be reached by almost all of the exit handling routines.
	 * If the ftok semaphore is grabbed, but not released, ftok_sem_reg will have a non-null value and grabbed_ftok_sem will be
	 * TRUE. We cannot rely on gv_cur_region always as it is used in so many places in so many ways.
	 * Note: Typically, it should suffice to check for ftok_sem_reg being non-null and pass that to ftok_sem_release. But, there
	 * are some cases where ftok_sem_reg can be NULL and yet jnlpool.jnlpool_dummy_reg is non-null and the process holds an ftok
	 * semaphore. For instance, if GT.M opened a particular region and then did a jnlpool_init which ended up with an rts_error
	 * (after obtaining the ftok semaphore on jnlpool_dummy_reg), gds_rundown done above (to rundown the database) sets
	 * ftok_sem_reg to NULL (as part of ftok_sem_release). But, jnlpool_dummy_reg is still non-null and the lingering ftok
	 * should be released. So, even though a subset of the below conditions should be enough, we check for all there cases just
	 * to be safe.
	 */
	if (ftok_sem_reg)
	{
		udi = FILE_INFO(ftok_sem_reg);
		assert(udi->grabbed_ftok_sem);
		ftok_sem_release(ftok_sem_reg, TRUE, TRUE);
	}
	if (NULL != jnlpool.jnlpool_dummy_reg)
	{
		udi = FILE_INFO(jnlpool.jnlpool_dummy_reg);
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(jnlpool.jnlpool_dummy_reg, TRUE, TRUE);
	}
	if (NULL != recvpool.recvpool_dummy_reg)
	{
		udi = FILE_INFO(recvpool.recvpool_dummy_reg);
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(recvpool.recvpool_dummy_reg, TRUE, TRUE);
	}
#	endif

	if (EXIT_NRM != rundown_status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBRNDWN);
}
