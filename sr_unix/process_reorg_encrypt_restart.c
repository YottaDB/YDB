/****************************************************************
 *								*
 * Copyright (c) 2015-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gdskill.h"
#include "gdsfhead.h"
#include "have_crit.h"
#include "process_reorg_encrypt_restart.h"

GBLREF	sgmnt_addrs	*reorg_encrypt_restart_csa;
#ifdef DEBUG
GBLREF	uint4		dollar_tlevel;
GBLREF	uint4		update_trans;
#endif

void process_reorg_encrypt_restart(void)
{
	intrpt_state_t	prev_intrpt_state;
	enc_info_t	*encr_ptr;
	int		gtmcrypt_errno;
	gd_segment	*seg;
	sgmnt_addrs	*csa;

	csa = reorg_encrypt_restart_csa;
	assert(NULL != csa);	/* caller should have ensured this */
	/* Opening handles for encryption is a heavyweight operation. Caller should have ensured we are not in crit for
	 * any region when the new key handles are opened for any one region. Assert that.
	 */
	assert(0 == have_crit(CRIT_HAVE_ANY_REG));
	DEFER_INTERRUPTS(INTRPT_IN_CRYPT_RECONFIG, prev_intrpt_state);
	encr_ptr = csa->encr_ptr;
	assert(NULL != encr_ptr);
	DBG_RECORD_CRYPT_RECEIVE(csa->hdr, csa, csa->nl, process_id, encr_ptr);
	seg = csa->region->dyn.addr;
	INIT_DB_OR_JNL_ENCRYPTION(csa, encr_ptr, seg->fname_len, seg->fname, gtmcrypt_errno);
	if (0 != gtmcrypt_errno)
	{
		ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_RECONFIG, prev_intrpt_state);
		GTMCRYPT_REPORT_ERROR(gtmcrypt_errno, rts_error, seg->fname_len, seg->fname);
	}
	reorg_encrypt_restart_csa = NULL;
	ENABLE_INTERRUPTS(INTRPT_IN_CRYPT_RECONFIG, prev_intrpt_state);
}
