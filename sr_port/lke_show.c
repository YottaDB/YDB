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
 * lke_show.c : displays locks for qualified regions
 * used in    : lke.c
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
#include "gtmmsg.h"
#include "min_max.h"
#include "interlock.h"
#include "rel_quant.h"
#include "do_shmat.h"
#include "mlk_ops.h"
#include "have_crit.h"

#define NOFLUSH_OUT	0
#define FLUSH		1
#define RESET		2

GBLREF	gd_addr		*gd_header;
GBLREF	uint4		process_id;
GBLREF	intrpt_state_t	intrpt_ok_state;

error_def(ERR_BADREGION);
error_def(ERR_LOCKCRITOWNER);
error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);
error_def(ERR_LOCKSPACEUSE);
error_def(ERR_NOLOCKMATCH);
error_def(ERR_NOREGION);
error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);


void	lke_show(void)
{
	bool			all = TRUE, interactive = FALSE, match = FALSE, memory = TRUE, nocrit = TRUE, wait = TRUE;
	boolean_t		exact = FALSE, locks, was_crit = FALSE;
	char 			nodebuf[32], one_lockbuf[MAX_KEY_SZ], regbuf[MAX_RN_LEN];
	float			ls_free = 0;	/* Free space in bottleneck subspace */
	gd_region		*reg;
	int			num_reg, shr_sub_len = 0;;
	int4			lcnt, pid;
	intrpt_state_t		prev_intrpt_state;
	mlk_ctldata_ptr_t	ctl;
	mlk_pvtctl		pctl, pctl2;
	mstr			node, one_lock, regname;
	sgmnt_addrs		*csa;
	size_t			ls_len;

	/* Get all command parameters */
	regname.addr = regbuf;
	regname.len = SIZEOF(regbuf);
	node.addr = nodebuf;
	node.len = SIZEOF(nodebuf);
	one_lock.addr = one_lockbuf;
	one_lock.len = SIZEOF(one_lockbuf);
	if (lke_getcli(&all, &wait, &interactive, &pid, &regname, &node, &one_lock, &memory, &nocrit, &exact, 0, 0) == 0)
		return;
	/* Search all regions specified on the command line */
	for (reg = gd_header->regions, num_reg = gd_header->n_regions; num_reg; ++reg, --num_reg)
	{
		/* If region matches and is open */
		if (((0 == regname.len) || ((reg->rname_len == regname.len) && !memcmp(reg->rname, regname.addr, regname.len)))
			&& reg->open)
		{
			match = TRUE;
			util_out_print("!/!AD!/", NOFLUSH_OUT, REG_LEN_STR(reg));
			/* If distributed database, the region is located on another node */
			if (reg->dyn.addr->acc_meth == dba_cm)
			{
#				if defined(LKE_WORKS_OK_WITH_CM)
				/* Obtain lock info from the remote node */
				locks = (boolean_t)gtcmtr_lke_showreq(reg->dyn.addr->cm_blk, reg->cmx_regnum,
							   all, wait, pid, &node);
				assert(FALSE);	/* because "csa" is not initialized here and is used below */
#				else
				gtm_putmsg_csa(NULL, VARLSTCNT(10) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
						LEN_AND_LIT("GT.CM region - locks must be displayed on the local node"),
						ERR_TEXT, 2, REG_LEN_STR(reg));
				continue;
#				endif
			} else if (IS_REG_BG_OR_MM(reg))
			{	/* non-remote region */
				csa = &FILE_INFO(reg)->s_addrs;
				ls_len = (size_t)csa->mlkctl_len;
				ctl = (mlk_ctldata_ptr_t)malloc(ls_len);
				MLK_PVTCTL_INIT(pctl, reg);
				if (nocrit)
				{	/* Set shrhash and shrhash_size here when nocrit, as they normally get set up
					 * when grabbing lock crit.
					 * If we have an external lock hash table, attach the shared memory.
					 */
					pctl.shrhash_size = pctl.ctl->num_blkhash;
					if (MLK_CTL_BLKHASH_EXT != pctl.ctl->blkhash)
						pctl.shrhash = (mlk_shrhash_ptr_t)R2A(pctl.ctl->blkhash);
					else
						pctl.shrhash = do_shmat(pctl.ctl->hash_shmid, NULL, 0);
				}
				/* Prevent any modification of the lock space while we make a local copy of it */
				for (lcnt = csa->mlkctl->max_prccnt; lcnt; lcnt--)
				{	/* this is a bit of a sleaze as show -nocrit might lose access to crit betewn if and grab */
					if ((0 == LOCK_CRIT_OWNER(csa)) || !nocrit)
					{	/* however, that should be harmless as a truely stuck crit would exhaust the loop */
						GRAB_LOCK_CRIT_AND_SYNC(&pctl, was_crit);
						DEFER_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
						break;
					}	/* a "cleaner" alternative involves modifying not only other code but interfaces */
					rel_quant();
				}
				memcpy((uchar_ptr_t)ctl, (uchar_ptr_t)csa->mlkctl, ls_len);
				assert((ctl->max_blkcnt > 0) && (ctl->max_prccnt > 0) && (ctl->subtop > ctl->subbase));
				pctl2 = pctl;
				if (MLK_CTL_BLKHASH_EXT == pctl.ctl->blkhash)
				{
					pctl2.shrhash = (mlk_shrhash_ptr_t)malloc(SIZEOF(mlk_shrhash) * pctl.shrhash_size);
					memcpy(pctl2.shrhash, pctl.shrhash, SIZEOF(mlk_shrhash) * pctl.shrhash_size);
				}
				if (nocrit)
				{
					if (LOCK_CRIT_HELD(csa))
					{
						REL_LOCK_CRIT(&pctl, was_crit);
						ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
					}
				} else if (MLK_CTL_BLKHASH_EXT == pctl.ctl->blkhash)
					SHMDT(pctl.shrhash);
				shr_sub_len = 0;
				MLK_PVTCTL_SET_CTL(pctl2, ctl);
				if (MLK_CTL_BLKHASH_EXT != pctl.ctl->blkhash)
					pctl2.shrhash = (mlk_shrhash_ptr_t)R2A(pctl2.ctl->blkhash);
				locks = (ctl->blkroot == 0) ? FALSE
					: lke_showtree(&pctl2, NULL, all, wait, pid, one_lock, memory, &shr_sub_len,
					nocrit ? NULL : &prev_intrpt_state);
				/* lock space usage consists of: control_block + nodes(locks) +  processes + substrings */
				/* any of those subspaces can be bottleneck.
				 * Therefore we will report the subspace which is running out.
				 */
				ls_free = MIN(((float)ctl->blkcnt) / ctl->max_blkcnt, ((float)ctl->prccnt) / ctl->max_prccnt);
				ls_free = MIN(1-(((float)shr_sub_len) / (ctl->subtop - ctl->subbase)), ls_free);
				ls_free *= 100;	/* Scale to [0-100] range. (couldn't do this inside util_out_print) */
				if (ls_free < 1) /* No memory? Notify user. */
					gtm_putmsg_csa(NULL, VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(reg));
				gtm_putmsg_csa(NULL, VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(reg),
					   (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
					   (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt,
					   shr_sub_len, (ctl->subtop - ctl->subbase));
				if (MLK_CTL_BLKHASH_EXT == pctl.ctl->blkhash)
					free(pctl2.shrhash);
				free(ctl);
				if (!nocrit)
				{
					REL_LOCK_CRIT(&pctl, was_crit);
					ENABLE_INTERRUPTS(INTRPT_IN_MLK_SHM_MODIFY, prev_intrpt_state);
				}
			} else
			{
				gtm_putmsg_csa(NULL, VARLSTCNT(2) ERR_BADREGION, 0);
				locks = TRUE;
				csa = NULL;
			}
			if (!locks)
				gtm_putmsg_csa(NULL, VARLSTCNT(4) ERR_NOLOCKMATCH, 2, REG_LEN_STR(reg));
			if (csa)
			{
				assert((ls_free <= 100) && (ls_free >= 0));
				gtm_putmsg_csa(csa, VARLSTCNT(4) ERR_LOCKSPACEUSE, 2, ((int)ls_free),
					       csa->hdr->lock_space_size/OS_PAGELET_SIZE);
				if (nocrit)
					gtm_putmsg_csa(csa, VARLSTCNT(3) ERR_LOCKCRITOWNER, 1, LOCK_CRIT_OWNER(csa));
			}
		}
	}
	if (!match && (0 != regname.len))
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_NOREGION, 2, regname.len, regname.addr);
}
