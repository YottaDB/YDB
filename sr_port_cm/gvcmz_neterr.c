/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "locklits.h"
#include "iotimer.h"
#include "gtm_string.h"
#include "gvcmy_close.h"
#include "gvcmz.h"
#include "op.h"
#include "dpgbldir.h"
#include "lv_val.h"		/* needed for "callg.h" */
#include "callg.h"

GBLREF	struct NTD	*ntd_root;
GBLDEF	bool		neterr_pending;

error_def(ERR_LCKSCANCELLED);

void	gvcmz_neterr(INTPTR_T *err)
{
	struct CLB	*p, *pn, *p1;
	unsigned char	*temp, buff[512];
	gd_addr		*gdptr;
	gd_region	*region, *r_top;
	uint4		count, lck_info;
	INTPTR_T	err_buff[10];
	boolean_t	locks = FALSE;

	neterr_pending = FALSE;
	if (NULL == ntd_root)
		GTMASSERT;
	for (p = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl);  p != (struct CLB *)ntd_root;  p = pn)
	{
		/* Get the forward link, in case a close removes the current entry */
		pn = (struct CLB *)RELQUE2PTR(p->cqe.fl);
		if (0 != ((link_info *)p->usr)->neterr)
		{
			p->ast = NULL;
			if (locks)
				gvcmy_close(p);
			else
			{
				locks = ((link_info *)p->usr)->lck_info & REMOTE_CLR_MASK;
				gvcmy_close(p);
				if (locks)
				{
					buff[0] = CMMS_L_LKCANALL;
					for (p1 = (struct CLB *)RELQUE2PTR(ntd_root->cqh.fl);
					     p1 != (struct CLB *)ntd_root;
					     p1 = (struct CLB *)RELQUE2PTR(p1->cqe.fl))
					{
						p1->ast = NULL;
						/* The following line effectively clears REQUEST_PENDING */
						lck_info = ((link_info *)p1->usr)->lck_info &= REMOTE_CLR_MASK;
						if (lck_info)
						{
							temp = p1->mbf;
							p1->mbf = buff;
							p1->cbl = S_HDRSIZE + S_LAFLAGSIZE;
							if (lck_info & (REMOTE_LOCKS | LREQUEST_SENT))
							{
								buff[1] = CM_LOCKS;
								cmi_write(p1);
							}
							if (lck_info & (REMOTE_ZALLOCATES | ZAREQUEST_SENT))
							{
								buff[1] = CM_ZALLOCATES;
								cmi_write(p1);
							}
							p1->mbf = temp;
						}
					}
					op_lkinit();
					op_unlock();
					op_zdeallocate(NO_M_TIMEOUT);
				}
			}
			/* Cycle through all active global directories */
			for (gdptr = get_next_gdr(NULL);  NULL != gdptr;  gdptr = get_next_gdr(gdptr))
				for (region = gdptr->regions, r_top = region + gdptr->n_regions;  region < r_top;  ++region)
					if ((dba_cm == region->dyn.addr->acc_meth) && (p == region->dyn.addr->cm_blk))
					{
						/* If it's a CM-accessed region via the current (error-generating) link: */
						region->open = FALSE;
						region->dyn.addr->acc_meth = dba_bg;
					}
		}
	}
	if (locks)
	{
		if (NULL != err)
		{
			count = (uint4)(*err + 1);
			memcpy(err_buff, err, count * SIZEOF(INTPTR_T));
			err_buff[count] = 0;
			err_buff[count + 1] = ERR_LCKSCANCELLED;
			err_buff[count + 2] = 0;
			err_buff[0] += 3;
			callg_signal(err_buff);
		} else
			rts_error(VARLSTCNT(1) ERR_LCKSCANCELLED);
	} else  if (NULL != err)
		callg_signal(err);

}
