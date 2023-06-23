/****************************************************************
 *								*
 * Copyright (c) 2004-2017 Fidelity National Information	*
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
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "t_abort.h"		/* for prototype of "t_abort" */
#include "process_reorg_encrypt_restart.h"
#include "op.h"			/* for OP_TROLLBACK */

GBLREF	sgmnt_addrs	*reorg_encrypt_restart_csa;
GBLREF	uint4		dollar_tlevel;
GBLREF	int		process_exiting;

void t_abort(gd_region *reg, sgmnt_addrs *csa)
{
#	ifdef DEBUG
	uint4		save_dollar_tlevel;
#	endif

	assert(!dollar_tlevel);
	assert(IS_REG_BG_OR_MM(reg));	/* caller should have ensured this */
	assert(&FILE_INFO(reg)->s_addrs == csa);
	if (process_exiting)
	{	/* If we are in the phase2 of an M-KILL (where "dollar_tlevel" is temporarily reset to 0), reset the global
		 * variable "bml_save_dollar_tlevel" and restore "dollar_tlevel" now that we are going to go into exit handling
		 * and unwind from that "gvcst_expand_free_subtree" context altogether.
		 */
		DEBUG_ONLY(save_dollar_tlevel = dollar_tlevel;)
		RESET_BML_SAVE_DOLLAR_TLEVEL;
		if (dollar_tlevel)
		{	/* If "dollar_tlevel" was 0 at function entry and became 1 now, it means we were in phase2 of
			 * an M-kill when we got a kill -15 and started exit handling and came here from "secshr_db_clnup".
			 * Exit handling code relies on "dollar_tlevel" being 0 (e.g. "gvcst_remove_statsDB_linkage_all").
			 * So rollback the TP.
			 */
			assert(!save_dollar_tlevel);
			OP_TROLLBACK(0);
			assert(!dollar_tlevel);
		}
	}
	t_abort_cleanup();
	/* Do not release crit in case of
	 * 	a) MUPIP RECOVER ONLINE  OR
	 * 	b) DSE where a CRIT SEIZE had been done on this region previously
	 * csa->hold_onto_crit is TRUE in both cases.
	 */
	if (csa->now_crit && !csa->hold_onto_crit)
		rel_crit(reg);
	/* If this transaction had a cdb_sc_reorg_encrypt restart, but we later decided to abort it, we still
	 * need to finish off opening the new encryption keys and clear the "reorg_encrypt_restart_csa" global.
	 */
	if (NULL != reorg_encrypt_restart_csa)
	{
		assert(csa == reorg_encrypt_restart_csa);
		process_reorg_encrypt_restart();
		assert(NULL == reorg_encrypt_restart_csa);
	}
}
