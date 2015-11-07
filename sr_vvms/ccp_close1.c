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

#include <fab.h>
#include <lckdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <efndef.h>


#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "jnl.h"
#include "locks.h"
#include "del_sec.h"
#include "mem_list.h"

#define PRIORITY 2

GBLREF	ccp_relque	*ccp_action_que;
GBLREF	ccp_db_header	*ccp_reg_root;
GBLREF	bool		ccp_stop;
GBLREF	int		ccp_stop_ctr;
GBLREF 	jnl_gbls_t	jgbl;

void			ccp_close_timeout();  /* TODO: move to a header */

static	int4		delta_1_sec[2] = { -10000000, -1 };


void	ccp_close1(ccp_db_header *db)
{
	ccp_db_header		*db0, *db1;
	ccp_que_entry		*que_ent;
	ccp_relque		*que_hd;
	mem_list		*ml_ptr, *ml_ptr_hold;
	sgmnt_addrs		*csa;
	vms_gds_info		*gds_info;
	unsigned char		section_name[GLO_NAME_MAXLEN];
	uint4			retadr[2], status, outaddrs[2];
	struct dsc$descriptor_s name_dsc;


	if (ccp_stop)
		ccp_stop_ctr--;

	if (db->stale_in_progress)
		sys$cantim(&db->stale_timer_id, PSL$C_USER);

	ccp_quemin_adjust(CCP_CLOSE_REGION);

	sys$cantim(&db->tick_timer_id, PSL$C_USER);
	sys$cantim(&db->quantum_timer_id, PSL$C_USER);

	db->segment->nl->ccp_state = CCST_CLOSED;
	db->wmexit_requested = TRUE;	/* ignore any blocking ASTs - already releasing */

	gds_info = FILE_INFO(db->greg);

	if (JNL_ENABLED(db->glob_sec))
	{
		if (db->segment->jnl != NULL  &&  db->segment->jnl->channel != 0)
		{
			status = sys$setimr(0, delta_1_sec, ccp_close_timeout, &db->close_timer_id, 0);
			if (status != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
			status = ccp_enqw(EFN$C_ENF, LCK$K_EXMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_NOQUEUE, NULL, 0,
					  NULL, 0, NULL, PSL$C_USER, 0);
			if (status == SS$_NOTQUEUED)
				/* We're not the only node accessing the journal file */
				jnl_file_close(db->greg, FALSE, FALSE);
			else
			{
				/***** Check error status here? *****/
				if (db->segment->jnl->jnl_buff->before_images  &&
				    db->segment->ti->curr_tn > db->segment->jnl->jnl_buff->epoch_tn)
				{
					csa = db->segment;
					JNL_SHORT_TIME(jgbl.gbl_jrec_time);	/* needed for jnl_put_jrt_pini()
											and jnl_write_epoch_rec() */
					if (0 == csa->jnl->pini_addr)
						jnl_put_jrt_pini(csa);
					db->segment->jnl->jnl_buff->epoch_tn = db->segment->ti->curr_tn;
					jnl_write_epoch_rec(db->segment);
				}
				jnl_file_close(db->greg, TRUE, FALSE);
			}
			sys$cantim(&db->close_timer_id, PSL$C_USER);
			status = gtm_deq(gds_info->s_addrs.jnl->jnllsb->lockid, NULL, PSL$C_USER, 0);
			if (status != SS$_NORMAL)
				ccp_signal_cont(status);	/***** Is this reasonable? *****/
		}
	}

	db->segment = NULL;	/* Warn AST's that the segment has been deleted */

	status = sys$deq(db->lock_iosb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
	if (status != SS$_NORMAL  &&  status != SS$_CANCELGRANT)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$deq(db->refcnt_iosb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
	if (status != SS$_NORMAL  &&  status != SS$_CANCELGRANT)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$deq(db->wm_iosb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
	if (status != SS$_NORMAL  &&  status != SS$_CANCELGRANT)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$deq(db->flush_iosb.lockid, NULL, PSL$C_USER, LCK$M_CANCEL);
	if (status != SS$_NORMAL  &&  status != SS$_CANCELGRANT)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$cancel(gds_info->fab->fab$l_stv);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$dassgn(gds_info->fab->fab$l_stv);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	csa = &gds_info->s_addrs;

	outaddrs[0] = csa->db_addrs[0] - OS_PAGE_SIZE;	/* header no access page */
	outaddrs[1] = csa->db_addrs[1] + OS_PAGE_SIZE;	/* trailer no access page */
	if (FALSE == is_va_free(outaddrs[0]))
		gtm_deltva(outaddrs, NULL, PSL$C_USER);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = sys$cretva(csa->db_addrs, retadr, PSL$C_USER);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/
	assert(retadr[0] == csa->db_addrs[0]  &&  retadr[1] == csa->db_addrs[1]);

	ml_ptr_hold=db->mem_ptr;

	if (ml_ptr_hold->prev != NULL)
	{
		/* if prior segment is adjacent and free, coalesce the segments */
		if (ml_ptr_hold->prev->free  &&
		    ml_ptr_hold->addr == ml_ptr_hold->prev->addr + OS_PAGELET_SIZE * ml_ptr_hold->prev->pages)
		{
			ml_ptr = ml_ptr_hold->prev;
			ml_ptr->next = ml_ptr_hold->next;
			if (ml_ptr->next != NULL)
				ml_ptr->next->prev = ml_ptr;
			ml_ptr->pages += ml_ptr_hold->pages;
			free(ml_ptr_hold);
			ml_ptr_hold = ml_ptr;
		}
	}

	if (ml_ptr_hold->next != NULL)
	{
		/* if next segment is adjacent and free, coalesce the segments */
		if (ml_ptr_hold->next->free  &&
		    ml_ptr_hold->next->addr == ml_ptr_hold->addr + OS_PAGELET_SIZE * ml_ptr_hold->pages)
		{
			ml_ptr = ml_ptr_hold->next;
			ml_ptr_hold->next = ml_ptr->next;
			if (ml_ptr_hold->next != NULL)
				ml_ptr_hold->next->prev = ml_ptr_hold;
			ml_ptr_hold->pages += ml_ptr->pages;
			free(ml_ptr);
		}
	}

	ml_ptr_hold->free = TRUE;

	global_name("GT$S", &gds_info->file_id, section_name);
	name_dsc.dsc$a_pointer = &section_name[1];
	name_dsc.dsc$w_length = section_name[0];
	name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	name_dsc.dsc$b_class = DSC$K_CLASS_S;
	status = del_sec(SEC$M_SYSGBL, &name_dsc, NULL);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	/* Dequeue locks after delete section in ccp_close,
	   acquire lock before create section in gvcst_init,
	   release lock after delete section in gds_rundown   */

	status = gtm_deq(db->lock_iosb.lockid, NULL, PSL$C_USER, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = gtm_deq(db->refcnt_iosb.lockid, NULL, PSL$C_USER, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = gtm_deq(db->wm_iosb.lockid, NULL, PSL$C_USER, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	status = gtm_deq(db->flush_iosb.lockid, NULL, PSL$C_USER, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	que_hd = &ccp_action_que[PRIORITY];
	for (que_ent = (char *)que_hd + que_hd->bl;  que_ent != que_hd;  que_ent = (char *)que_ent + que_ent->q.bl)
		if (que_ent->value.v.h == db)
			que_ent->value.v.h = 0;

	free(gds_info->fab->fab$l_nam);
	free(gds_info->fab);
	free(db->greg->dyn.addr);
	free(db->greg);

	/* Remove db from list, this list should never be changed in an AST */
	for (db0 = ccp_reg_root, db1 = NULL;  db0 != db;  db1 = db0, db0 = db0->next)
		;
	if (db1 == NULL)
		ccp_reg_root = db0->next;
	else
		db1->next = db0->next;

	free(db);

	return;
}
