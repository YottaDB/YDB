/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <descrip.h>
#include <lckdef.h>
#include <lkidef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>

#include "gdsroot.h"
#include "ladef.h"
#include "vmsdtype.h"
#include "locks.h"
#include "gtm_getlkiw.h"

/*
   lp_acquire.c : lp_acquire = SS$_NORMAL && (lkid, lid) already acquired
		  lp_acquire = SS$_NORMAL && 0 != lkid && units available
		  lp_acquire = LP_JOBLIM  && 0 == lkid && no units available
		  lp_acquire = system call error
   used in      : Licensed products
 */

GBLREF	int4		process_id;

error_def		(LP_JOBLIM);	/* Job limit exceeded	*/


static	const	int4	mode[] = { LCK$K_CWMODE, LCK$K_CWMODE, LCK$K_EXMODE };

int4	lp_acquire(pak *p, int4 lval, int4 lid, int4 *lkid)
	/* *p => pak record [NOTE: not used!] */
	/* lval => license value */
	/* lid -> license ID */
	/* returns:  lkid => lock ID */
{
	unsigned short		iosb[4];
	uint4			status;
	int4			i, rsbrefcnt, retlen;
	vms_lock_sb		lksb[3];
	struct dsc$descriptor_s	dres[] =
	{
		{ SIZEOF(LMLK) - 1, DSC$K_DTYPE_T, DSC$K_CLASS_S, LMLK },
		{ SIZEOF(int4), DSC$K_DTYPE_T, DSC$K_CLASS_S, &lid },
		{ SIZEOF(int4), DSC$K_DTYPE_T, DSC$K_CLASS_S, &process_id }
	};
	struct
	{
		item_list_3	ilist;
		int4		terminator;
	} item_list =
	{
		{ SIZEOF(int4), LKI$_RSBREFCNT, &rsbrefcnt, &retlen },
		0
	};


	status = lp_confirm(lid, *lkid);
	if (SS$_NORMAL != status)
	{
		*lkid = 0;
		if (0 == lid)
			return LP_JOBLIM;

		for (i = 0;  i < 3;  ++i)
		{
			status = gtm_enqw(EFN$C_ENF, mode[i], &lksb[i], LCK$M_SYSTEM, &dres[i], *lkid,
						NULL, 0, NULL, PSL$C_USER, 0);
			if (SS$_NORMAL == status)
				status = lksb[i].cond;
			if (SS$_NORMAL != status)
				return status;
			*lkid = lksb[i].lockid;
		}

		status = gtm_getlkiw(EFN$C_ENF, &lksb[1].lockid, &item_list, iosb, NULL, 0, 0);
		if (SS$_NORMAL == status)
			status = iosb[0];
		if ((SS$_NORMAL == status) && (rsbrefcnt > lval))
			status = LP_JOBLIM;
	}

	return status;
}
