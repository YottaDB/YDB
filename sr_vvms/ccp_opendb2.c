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

#include <fab.h>
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "ccp.h"
#include "mlkdef.h"
#include "jnl.h"
#include "locks.h"
#include <descrip.h>
#include <iodef.h>
#include <lckdef.h>
#include <psldef.h>
#include <secdef.h>
#include <ssdef.h>
#include "min_max.h"
#include "mem_list.h"
#include "init_sec.h"

OS_PAGE_SIZE_DECLARE

GBLREF	mem_list	*mem_list_head;


/* AST routine entered on completion of PW mode lock conversion request
   in ccp_tr_opendb1, ccp_tr_opendb1b, or here */

void ccp_opendb2(ccp_db_header *db)
{
	uint4		status;
	unsigned char		section_name[GLO_NAME_MAXLEN];
	struct dsc$descriptor_s name_dsc;
	int4			size;
	sgmnt_addrs		*csa;
	sgmnt_data		*csd;
	mem_list		*ml_ptr;


	assert(lib$ast_in_prog());

	if (db->wm_iosb.cond != SS$_NORMAL  &&  db->wm_iosb.cond != SS$_VALNOTVALID)
	{
		ccp_signal_cont(db->wm_iosb.cond);	/***** Is this reasonable? *****/

		if (db->wm_iosb.cond == SS$_DEADLOCK)
		{
			/* Just try again */
			/* Convert Write-mode lock from Null to Protected Write, reading the lock value block */
			status = ccp_enq(0, LCK$K_PWMODE, &db->wm_iosb, LCK$M_CONVERT | LCK$M_VALBLK, NULL, 0,
					 ccp_opendb2, db, NULL, PSL$C_USER, 0);
			/***** Check error status here? *****/
			return;
		}
	}

	csa = db->segment;
	if (csa->hdr->lock_space_size == 0)
		if (csa->hdr->n_bts * csa->hdr->blk_size <= DEF_LOCK_SIZE)
			csa->hdr->lock_space_size = csa->hdr->n_bts * csa->hdr->blk_size;
		else
		{
			csa->hdr->lock_space_size = DEF_LOCK_SIZE;

			/* REPLACE THIS WITH FREE_SPACE WHEN NEXT BASELINE MADE */
			csa->hdr->free_space = csa->hdr->n_bts * csa->hdr->blk_size - DEF_LOCK_SIZE;
		}

	size = DIVIDE_ROUND_UP((LOCK_BLOCK(csa->hdr) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(csa->hdr) + CACHE_CONTROL_SIZE(csa->hdr)
			+ NODE_LOCAL_SPACE(csa->hdr) + JNL_SHARE_SIZE(csa->hdr), OS_PAGELET_SIZE);

	status = gtm_expreg(size, csa->db_addrs, PSL$C_USER, 0);
	if (status != SS$_NORMAL)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	assert(csa->db_addrs[1] == csa->db_addrs[0] + OS_PAGELET_SIZE * size - 1);
	csa->db_addrs[1] = csa->db_addrs[0] + OS_PAGELET_SIZE * (size - OS_PAGE_SIZE/OS_PAGELET_SIZE) - 1;

	for (ml_ptr = mem_list_head; ml_ptr != NULL; ml_ptr = ml_ptr->next)
		if (csa->db_addrs[0] == ml_ptr->addr)
			break;
	assert (ml_ptr);
	db->mem_ptr = ml_ptr;
	global_name("GT$S", &FILE_INFO(db->greg)->file_id, section_name);
	name_dsc.dsc$a_pointer = &section_name[1];
	name_dsc.dsc$w_length = section_name[0];
	name_dsc.dsc$b_dtype = DSC$K_DTYPE_T;
	name_dsc.dsc$b_class = DSC$K_CLASS_S;
	status = init_sec(csa->db_addrs, &name_dsc, 0, size - OS_PAGE_SIZE/OS_PAGELET_SIZE,
			  SEC$M_GBL | SEC$M_DZRO | SEC$M_WRT | SEC$M_PAGFIL | SEC$M_SYSGBL | SEC$M_PERM);
	if (status != SS$_NORMAL  &&  status != SS$_CREATED)
		ccp_signal_cont(status);	/***** Is this reasonable? *****/

	memcpy(csa->db_addrs[0], csa->hdr, SIZEOF(sgmnt_data));
	free (csa->hdr);
	csa->hdr = csa->db_addrs[0];

	csa->critical = csa->db_addrs[0] + (LOCK_BLOCK(csa->hdr) * DISK_BLOCK_SIZE) + LOCK_SPACE_SIZE(csa->hdr)
			+ CACHE_CONTROL_SIZE(csa->hdr);
	assert((-(SIZEOF(int4) * 2) & (uint4)csa->critical) == (uint4)csa->critical);	/* DB64 */
	mutex_init(csa->critical, NUM_CRIT_ENTRY(csa->hdr), FALSE);

	free(csa->nl);
	csa->nl = (char *)csa->critical + CRIT_SPACE(NUM_CRIT_ENTRY(csa->hdr));
	csa->nl->ccp_state = CCST_OPNREQ;

	status = sys$qio(0, FILE_INFO(db->greg)->fab->fab$l_stv, IO$_READVBLK, &db->qio_iosb, ccp_opendb3, db,
			 &csa->hdr->trans_hist, BT_SIZE(csa->hdr) + SIZEOF(th_index), TH_BLOCK, 0, 0, 0);
	if ((status & 1) == 0)
	{
		ccp_signal_cont(status);	/***** Is this reasonable? *****/
		ccp_close1(db);
	}

	return;
}
