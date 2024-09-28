/****************************************************************
 *								*
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -------------------------------------------------
 * lke_clear.c : removes locks for qualified regions
 * used in     : lke.c
 * -------------------------------------------------
 */
#include <sys/shm.h>

#include "mdef.h"

#include "gtm_string.h"

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
#include "gtcmtr_protos.h"
#include "lke.h"
#include "lke_getcli.h"
#include "lke_cleartree.h"
#include "gtmmsg.h"
#include "interlock.h"
#include "rel_quant.h"
#include "do_shmat.h"
#include "mlk_ops.h"
#include "restrict.h"
#include "have_crit.h"

#define NOFLUSH_OUT	0
#define FLUSH		1
#define RESET		2

GBLREF	intrpt_state_t	intrpt_ok_state;
GBLREF	gd_addr		*gd_header;

error_def(ERR_BADREGION);
error_def(ERR_NOLOCKMATCH);
error_def(ERR_NOREGION);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);

void	lke_clear(void)
{
	bool		all = TRUE, interactive = TRUE, locks, match = FALSE, memory = FALSE, nocrit = FALSE, wait = FALSE;
	boolean_t	exact = TRUE, was_crit = FALSE;
	char		nodebuf[32], one_lockbuf[MAX_KEY_SZ], regbuf[MAX_RN_LEN];
	gd_region	*reg;
	int		num_reg;
	int4		pid;
	intrpt_state_t	prev_intrpt_state;
	mlk_pvtctl	pctl;
	mstr		node, one_lock, regname;

	if (RESTRICTED(lkeclear))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "LKECLEAR");
	/* Get all command parameters */
	regname.addr = regbuf;
	regname.len = SIZEOF(regbuf);
	node.addr = nodebuf;
	node.len = SIZEOF(nodebuf);
	one_lock.addr = one_lockbuf;
	one_lock.len = SIZEOF(one_lockbuf);
	if (0 == lke_getcli(&all, &wait, &interactive, &pid, &regname, &node, &one_lock, &memory, &nocrit, &exact, 0, 0))
		return;
	/* Search all regions specified on the command line */
	for (reg = gd_header->regions, num_reg = gd_header->n_regions; num_reg; ++reg, --num_reg)
	{	/* If region matches and is open */
		if (((0 == regname.len) || ((reg->rname_len == regname.len) && !memcmp(reg->rname, regname.addr, regname.len)))
			&& reg->open)
		{
			match = TRUE;
			util_out_print("!/!AD!/", NOFLUSH_OUT, REG_LEN_STR(reg));
			/* If distributed database, the region is located on another node */
			if (reg->dyn.addr->acc_meth == dba_cm)
			{
#				if defined(LKE_WORKS_OK_WITH_CM)
				/* Remote lock clears are not supported, so LKE CLEAR -EXACT qualifier
				 * will not be supported on GT.CM.*/
				locks = gtcmtr_lke_clearreq(reg->dyn.addr->cm_blk, reg->cmx_regnum,
							    all, interactive, pid, &node);
#				else
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(10) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
						LEN_AND_LIT("GT.CM region - locks must be cleared on the local node"),
						ERR_TEXT, 2, REG_LEN_STR(reg));
				continue;
#				endif
			} else if (IS_REG_BG_OR_MM(reg))
			{	/* Local region */
				MLK_PVTCTL_INIT(pctl, reg);
				/* Prevent any modifications of locks while we are clearing */
				GRAB_LOCK_CRIT_AND_SYNC(&pctl, was_crit);
				DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
				locks = pctl.ctl->blkroot == 0 ? FALSE
					: lke_cleartree(&pctl, NULL,  (mlk_shrblk_ptr_t)R2A(pctl.ctl->blkroot),
						all, interactive, pid, one_lock, exact, &prev_intrpt_state);
				REL_LOCK_CRIT(&pctl, was_crit);
				ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
			} else
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(2) ERR_BADREGION, 0);
				locks = TRUE;
			}
			if (!locks)
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOLOCKMATCH, 2, REG_LEN_STR(reg));
		}
	}
	if (!match  &&  regname.len != 0)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NOREGION, 2, regname.len, regname.addr);
}
