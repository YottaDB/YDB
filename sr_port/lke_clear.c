/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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

#define NOFLUSH 0
#define FLUSH	1
#define RESET	2

GBLREF	gd_addr		*gd_header;
GBLREF	short		crash_count;

error_def(ERR_NOREGION);
error_def(ERR_UNIMPLOP);
error_def(ERR_TEXT);
error_def(ERR_BADREGION);
error_def(ERR_NOLOCKMATCH);

void	lke_clear(void)
{
	bool		locks, all = TRUE, wait = FALSE, interactive = TRUE, match = FALSE, memory = FALSE, nocrit = FALSE;
	boolean_t	exact = TRUE, was_crit = FALSE;
	int4		pid;
	int		n;
	char		regbuf[MAX_RN_LEN], nodebuf[32], one_lockbuf[MAX_KEY_SZ];
	mstr		regname, node, one_lock;
	gd_region	*reg;
	mlk_pvtctl	pctl;

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
	for (reg = gd_header->regions, n = 0; n != gd_header->n_regions; ++reg, ++n)
	{	/* If region matches and is open */
		if (((0 == regname.len) || (reg->rname_len == regname.len) && (0 == memcmp(reg->rname, regname.addr, regname.len)))
			&& reg->open)
		{
			match = TRUE;
			util_out_print("!/!AD!/", NOFLUSH, REG_LEN_STR(reg));
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
				GRAB_LOCK_CRIT_AND_SYNC(pctl, was_crit);
				locks = (0 == pctl.ctl->blkroot) ? FALSE
							  : lke_cleartree(&pctl, NULL,
									  (mlk_shrblk_ptr_t)R2A(pctl.ctl->blkroot),
									  all, interactive, pid, one_lock, exact);
				REL_LOCK_CRIT(pctl, was_crit);
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
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, regname.len, regname.addr);
}
