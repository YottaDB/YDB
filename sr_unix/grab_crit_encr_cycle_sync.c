/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information 		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_string.h"

#include "cdb_sc.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "process_reorg_encrypt_restart.h"

GBLREF	sgmnt_addrs	*reorg_encrypt_restart_csa;
GBLREF	uint4		mu_reorg_encrypt_in_prog;	/* non-zero if MUPIP REORG ENCRYPT is in progress */
GBLREF	uint4		process_id;
#ifdef DEBUG
GBLREF	uint4		dollar_tlevel;
#endif

/* This is a version of "grab_crit" that returns with crit held but also ensures
 * "csa->nl->reorg_encrypt_cycle == csa->encr_ptr->reorg_encrypt_cycle" and that the encryption
 * sync happens without holding crit (as that can take a lot of time involving external access to gpg).
 * This should be used by
 *	1) Callers of "t_qread" who expect that holding crit and doing t_qread is guaranteed to not return a NULL value
 *			(NULL is possible if the encryption cycles are different). Examples of such callers are
 *		a) All dse*.c modules
 *		b) mu_reorg_upgrd_dwngrd.c when it does a t_qread of a bitmap block inside crit
 *		c) mupip_reorg_encrypt.c when it does a t_qread of a bitmap block inside crit
 *		d) gvcst_expand_free_subtree.c when it does a t_qread of a killed index block to find its descendants to kill.
 *	2) Functions like "t_retry" and "tp_restart" which grab_crit to enter into the final retry. It is best to do
 *		the heavyweight new-encryption-handle opening operation while outside of crit.
 *
 * Returns: TRUE if new-encryption-handles were opened (i.e. "process_reorg_encrypt_restart" was invoked) at least once.
 *          FALSE otherwise.
 */
boolean_t grab_crit_encr_cycle_sync(gd_region *reg)
{
	boolean_t		sync_needed;
	enc_info_t		*encr_ptr;
	enum cdb_sc		status;
	node_local_ptr_t	cnl;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	unix_db_info		*udi;

	udi = FILE_INFO(reg);
	csa = &udi->s_addrs;
	grab_crit(reg);
	encr_ptr = csa->encr_ptr;
	sync_needed = FALSE;
	if (NULL != encr_ptr)
	{
		csd = csa->hdr;
		cnl = csa->nl;
		assert(NULL == reorg_encrypt_restart_csa);
		while (cnl->reorg_encrypt_cycle != encr_ptr->reorg_encrypt_cycle)
		{
			sync_needed = TRUE;
			/* Cycles mismatch. Fix the cycles. Take copy of shared memory while holding crit but do the
			 * "process_encrypt_restart_csa" after releasing crit as that is a heavyweight operation
			 * (involving access to external "gpg").
			 */
			SIGNAL_REORG_ENCRYPT_RESTART(mu_reorg_encrypt_in_prog, reorg_encrypt_restart_csa,
					cnl, csa, csd, status, process_id);
			rel_crit(reg);
			assert(csa == reorg_encrypt_restart_csa);
			process_reorg_encrypt_restart();
			assert(NULL == reorg_encrypt_restart_csa);
			grab_crit(reg);
		}
	}
	return sync_needed;
}
