/****************************************************************
 *								*
 * Copyright (c) 2019-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include <sys/shm.h>

#include "mdef.h"
#include "mlkdef.h"
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cmidef.h"	/* for cmmdef.h */
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"	/* for gtcmtr_protos.h */
#include "util.h"
#include "lke.h"
#include "lke_getcli.h"
#include "mlk_rehash.h"
#include "interlock.h"
#include "sleep.h"
#include "min_max.h"
#include "gtmmsg.h"
#include "do_shmat.h"
#include "mlk_ops.h"
#include "have_crit.h"

GBLREF	gd_addr		*gd_header;
GBLREF	intrpt_state_t	intrpt_ok_state;

error_def(ERR_NOREGION);

void lke_rehash(void)
{
	/* Arguments for lke_getcli */
	bool			all = TRUE, interactive = FALSE, match = FALSE, memory = TRUE, nocrit = TRUE, wait = TRUE;
	boolean_t		exact = TRUE, was_crit = FALSE;
	char			nodebuf[32], one_lockbuf[MAX_KEY_SZ], regbuf[MAX_RN_LEN];
	gd_region		*reg;
	int			num_reg;
	int4			pid;
	intrpt_state_t		prev_intrpt_state;
	mlk_pvtctl		pctl;
	mstr			node, one_lock, regname;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	regname.addr = regbuf;
	regname.len = SIZEOF(regbuf);
	node.addr = nodebuf;
	node.len = SIZEOF(nodebuf);
	one_lock.addr = one_lockbuf;
	one_lock.len = SIZEOF(one_lockbuf);
	if (lke_getcli(&all, &wait, &interactive, &pid, &regname, &node, &one_lock, &memory, &nocrit, &exact,
				NULL, NULL) == 0)
		return;
	for (reg = gd_header->regions, num_reg = gd_header->n_regions; num_reg ; ++reg, --num_reg)
	{
		/* If region matches and is open */
		if (((0 == regname.len)
				|| ((reg->rname_len == regname.len) && !memcmp(reg->rname, regname.addr, regname.len)))
			&& reg->open)
		{
			assert(IS_REG_BG_OR_MM(reg));
			if (IS_STATSDB_REGNAME(reg))
				continue;
			match = TRUE;
			/* Construct a dummy pctl to pass in */
			MLK_PVTCTL_INIT(pctl, reg);
			GRAB_LOCK_CRIT_AND_SYNC(&pctl, was_crit);
			DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
			mlk_rehash(&pctl);
			REL_LOCK_CRIT(&pctl, was_crit);
			ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
			util_out_print("Rehash of lock completed for region !AD (seed = !UJ)", TRUE,
						REG_LEN_STR(reg), pctl.ctl->hash_seed);
		}
	}
	if (!match && (0 != regname.len))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NOREGION, 2, regname.len, regname.addr);
}
