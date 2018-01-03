/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>

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
#include "io.h"
#include "gtmsecshr.h"
#include "mutex.h"
#include "ftok_sems.h"
#include "tp_change_reg.h"
#include "gds_rundown.h"
#include "dpgbldir.h"
#include "gvcmy_rundown.h"
#include "rc_cpt_ops.h"
#include "gv_rundown.h"
#include "targ_alloc.h"
#ifdef DEBUG
#include "anticipatory_freeze.h"
#endif
#include "aio_shim.h"

GBLREF	gd_region		*gv_cur_region;
GBLREF	sgmnt_addrs		*cs_addrs;
GBLREF	int			pool_init;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;
GBLREF	jnlpool_addrs_ptr_t	jnlpool_head;
GBLREF	recvpool_addrs		recvpool;
GBLREF	gd_region		*ftok_sem_reg;
#ifdef DEBUG
GBLREF	boolean_t		is_jnlpool_creator;
error_def(ERR_TEXT);
#endif
error_def(ERR_NOTALLDBRNDWN);

void gv_rundown(void)
{
	gd_region		*r_top, *r_save, *r_local;
	gd_addr			*addr_ptr;
	jnlpool_addrs_ptr_t	local_jnlpool, save_jnlpool;
	sgm_info		*si;
	int4			rundown_status = EXIT_NRM;	/* if gds_rundown went smoothly */
	unix_db_info		*udi;
	sgmnt_addrs		*csa;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	r_save = gv_cur_region;		/* Save for possible core dump */
	save_jnlpool = jnlpool;
	gvcmy_rundown();
	ENABLE_AST
	if (pool_init)
	{
		for (local_jnlpool = jnlpool_head; local_jnlpool; local_jnlpool = local_jnlpool->next)
			if (local_jnlpool->pool_init)
				rel_lock(local_jnlpool->jnlpool_dummy_reg);
	}
	for (addr_ptr = get_next_gdr(NULL); addr_ptr; addr_ptr = get_next_gdr(addr_ptr))
	{
		for (r_local = addr_ptr->regions, r_top = r_local + addr_ptr->n_regions; r_local < r_top; r_local++)
		{
			if (r_local->open && (dba_cm != r_local->dyn.addr->acc_meth))
			{
				/* Rundown has already occurred for GT.CM client regions through gvcmy_rundown() above.
			 	 * Hence the (dba_cm != ...) check in the if above. Note that for GT.CM client regions,
				 * region->open is TRUE although cs_addrs is NULL.
			 	 */
#				ifdef DEBUG
				if (is_jnlpool_creator
					&& INST_FREEZE_ON_NOSPC_ENABLED(REG2CSA(r_local), local_jnlpool)
					&& TREF(gtm_test_fake_enospc))
				{	/* Clear ENOSPC faking now that we are running down */
					csa = REG2CSA(r_local);
					assert((NULL == csa->jnlpool) || (jnlpool == csa->jnlpool));
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
				csa = REG2CSA(r_local);
				if (csa->jnlpool && (jnlpool != csa->jnlpool))
					jnlpool = csa->jnlpool;
			        tp_change_reg();
				rundown_status |= gds_rundown(CLEANUP_UDI_TRUE);
			}
			r_local->open = r_local->was_open = FALSE;
		}
	}
	rc_close_section();
	gv_cur_region = r_save;		/* Restore value for dumps but this region is now closed and is otherwise defunct */
	jnlpool = save_jnlpool;
	cs_addrs = NULL;
	gtmsecshr_sock_cleanup(CLIENT);
#	ifndef MUTEX_MSEM_WAKE
	mutex_sock_cleanup();
#	endif
	for (jnlpool = jnlpool_head; jnlpool; jnlpool = jnlpool->next)
		jnlpool_detach();
	/* Clean up any left-over ftok semaphores. This part of the code can be reached by almost all of the exit handling routines.
	 * If the ftok semaphore is grabbed, but not released, ftok_sem_reg will have a non-null value and grabbed_ftok_sem will be
	 * TRUE. We cannot rely on gv_cur_region always as it is used in so many places in so many ways.
	 * Note: Typically, it should suffice to check for ftok_sem_reg being non-null and pass that to ftok_sem_release. But, there
	 * are some cases where ftok_sem_reg can be NULL and yet jnlpool->jnlpool_dummy_reg is non-null and the process holds an
	 * ftok semaphore. For instance, if GT.M opened a particular region and then did a jnlpool_init which ended up with an
	 * rts_erro (after obtaining the ftok semaphore on jnlpool_dummy_reg), gds_rundown done above (to rundown the database) sets
	 * ftok_sem_reg to NULL (as part of ftok_sem_release). But, jnlpool_dummy_reg is still non-null and the lingering ftok
	 * should be released. So, even though a subset of the below conditions should be enough, we check for all there cases just
	 * to be safe.
	 * Note that we use FALSE for the decr_cnt parameter (2nd parameter) to "ftok_sem_release". This is to avoid incorrect
	 * removal of the ftok semaphore in case the counter is down to 1 but there are processes which did not bump the counter
	 * (due to the counter overflowing) that are still accessing the semaphore. Even though we don't decrement the counter,
	 * the SEM_UNDO will take care of doing the actual decrement when this process terminates. The only consequence is
	 * we will not be removing the ftok semaphore when the last process to use it dies (requiring a mupip rundown to clean
	 * it up). But that is considered okay since these are abnormal exit conditions anyways and hopefully unlikely in practice.
	 */
	if (ftok_sem_reg)
	{
		udi = FILE_INFO(ftok_sem_reg);
		assert(udi->grabbed_ftok_sem);
		ftok_sem_release(ftok_sem_reg, FALSE, TRUE);
	}
	for (jnlpool = jnlpool_head; jnlpool; jnlpool = jnlpool->next)
		if (jnlpool->jnlpool_dummy_reg && jnlpool->pool_init)
		{
			udi = FILE_INFO(jnlpool->jnlpool_dummy_reg);
			if (udi->grabbed_ftok_sem)
				ftok_sem_release(jnlpool->jnlpool_dummy_reg, FALSE, TRUE);
		}
	if (NULL != recvpool.recvpool_dummy_reg)
	{
		udi = FILE_INFO(recvpool.recvpool_dummy_reg);
		if (udi->grabbed_ftok_sem)
			ftok_sem_release(recvpool.recvpool_dummy_reg, FALSE, TRUE);
	}
	if (EXIT_NRM != rundown_status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_NOTALLDBRNDWN);
}
