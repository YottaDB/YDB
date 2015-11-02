/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include "longcpy.h"
#include "gtcmtr_protos.h"
#include "lke.h"
#include "lke_getcli.h"
#include "gtmmsg.h"
#include "min_max.h"

#define NOFLUSH 0
#define FLUSH	1
#define RESET	2

GBLREF	gd_region	*gv_cur_region;
GBLREF	gd_addr		*gd_header;
GBLREF	sgmnt_addrs	*cs_addrs;
GBLREF	short		crash_count;

error_def(ERR_UNIMPLOP);
error_def(ERR_TEXT);
error_def(ERR_NOREGION);
error_def(ERR_BADREGION);
error_def(ERR_NOLOCKMATCH);
error_def(ERR_LOCKSPACEUSE);
error_def(ERR_LOCKSPACEFULL);
error_def(ERR_LOCKSPACEINFO);

void	lke_show(void)
{
	bool			locks, all = TRUE, wait = TRUE, interactive = FALSE, match = FALSE, memory = TRUE, nocrit = TRUE;
	boolean_t		exact = FALSE, was_crit;
	int4			pid;
	size_t			ls_len;
	int			n;
	char 			regbuf[MAX_RN_LEN], nodebuf[32], one_lockbuf[MAX_KEY_SZ];
	mlk_ctldata_ptr_t	ctl;
	mstr			reg, node, one_lock;
	int			shr_sub_len = 0;
	float			ls_free = 0;	/* Free space in bottleneck subspace */
	/* Get all command parameters */
	reg.addr = regbuf;
	reg.len = SIZEOF(regbuf);
	node.addr = nodebuf;
	node.len = SIZEOF(nodebuf);
	one_lock.addr = one_lockbuf;
	one_lock.len = SIZEOF(one_lockbuf);
	if (lke_getcli(&all, &wait, &interactive, &pid, &reg, &node, &one_lock, &memory, &nocrit, &exact) == 0)
		return;

	/* Search all regions specified on the command line */
	for (gv_cur_region = gd_header->regions, n = 0; n != gd_header->n_regions; ++gv_cur_region, ++n)
	{
		/* If region matches and is open */
		if ((reg.len == 0  ||
		     gv_cur_region->rname_len == reg.len  &&  memcmp(gv_cur_region->rname, reg.addr, reg.len) == 0)  &&
		    gv_cur_region->open)
		{
			match = TRUE;
			util_out_print("!/!AD!/", NOFLUSH, REG_LEN_STR(gv_cur_region));

			/* If distributed database, the region is located on another node */
			if (gv_cur_region->dyn.addr->acc_meth == dba_cm)
			{
#				if defined(LKE_WORKS_OK_WITH_CM)
				/* Obtain lock info from the remote node */
				locks = gtcmtr_lke_showreq(gv_cur_region->dyn.addr->cm_blk, gv_cur_region->cmx_regnum,
							   all, wait, pid, &node);
#				else
				gtm_putmsg(VARLSTCNT(10) ERR_UNIMPLOP, 0, ERR_TEXT, 2,
						LEN_AND_LIT("GT.CM region - locks must be displayed on the local node"),
						ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
				continue;
#				endif
			} else if (gv_cur_region->dyn.addr->acc_meth == dba_bg  || gv_cur_region->dyn.addr->acc_meth == dba_mm)
			{	/* Local region */
				cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
				ls_len = (size_t)(cs_addrs->lock_addrs[1] - cs_addrs->lock_addrs[0]);
				ctl = (mlk_ctldata_ptr_t)malloc(ls_len);
				/* Prevent any modification of the lock space while we make a local copy of it */
				if (cs_addrs->critical != NULL)
					crash_count = cs_addrs->critical->crashcnt;
				was_crit = cs_addrs->now_crit;
				if (!nocrit && !was_crit)
					grab_crit(gv_cur_region);
				longcpy((uchar_ptr_t)ctl, (uchar_ptr_t)cs_addrs->lock_addrs[0], ls_len);
				assert((ctl->max_blkcnt > 0) && (ctl->max_prccnt > 0) && ((ctl->subtop - ctl->subbase) > 0));
				if (!nocrit && !was_crit)
					rel_crit(gv_cur_region);
				shr_sub_len = 0;
				locks = ctl->blkroot == 0 ?
						FALSE:
						lke_showtree(NULL, (mlk_shrblk_ptr_t)R2A(ctl->blkroot), all, wait, pid,
							     one_lock, memory, &shr_sub_len);
				/* lock space usage consists of: control_block + nodes(locks) +  processes + substrings */
				/* any of those subspaces can be bottleneck.
				 * Therefore we will report the subspace which is running out.
				 */
				ls_free = MIN(((float)ctl->blkcnt) / ctl->max_blkcnt, ((float)ctl->prccnt) / ctl->max_prccnt);
				ls_free = MIN(1-(((float)shr_sub_len) / (ctl->subtop - ctl->subbase)), ls_free);
				ls_free *= 100;	/* Scale to [0-100] range. (couldn't do this inside util_out_print) */
				if (ls_free < 1) /* No memory? Notify user. */
					gtm_putmsg(VARLSTCNT(4) ERR_LOCKSPACEFULL, 2, DB_LEN_STR(gv_cur_region));
				if (ls_free < 1 || memory)
				{
					if (ctl->subtop > ctl->subfree)
						gtm_putmsg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(gv_cur_region),
							   (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
							   (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" not "));
					else
						gtm_putmsg(VARLSTCNT(10) ERR_LOCKSPACEINFO, 8, REG_LEN_STR(gv_cur_region),
							   (ctl->max_prccnt - ctl->prccnt), ctl->max_prccnt,
							   (ctl->max_blkcnt - ctl->blkcnt), ctl->max_blkcnt, LEN_AND_LIT(" "));
				}
				free(ctl);
			} else
			{
				gtm_putmsg(VARLSTCNT(2) ERR_BADREGION, 0);
				locks = TRUE;
			}
			if (!locks)
			{
				gtm_putmsg(VARLSTCNT(4) ERR_NOLOCKMATCH, 2, REG_LEN_STR(gv_cur_region));
			}
			assert((ls_free <= 100) && (ls_free >= 0));
			gtm_putmsg(VARLSTCNT(4) ERR_LOCKSPACEUSE, 2, ((int)ls_free),
				       cs_addrs->hdr->lock_space_size/OS_PAGELET_SIZE);
		}
	}

	if (!match  &&  reg.len != 0)
		rts_error(VARLSTCNT(4) ERR_NOREGION, 2, reg.len, reg.addr);

}
