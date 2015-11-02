/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
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
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"	/* for muprec.h */
#include "buddy_list.h"	/* for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "iosp.h"		/* for SS_NORMAL */

/* This routine marks the state of process vector for show=BROKEN/ACTIVE/PROCESS
 * Note that unfenced transactions can be broken only in the case -FENCES=ALWAYS
 * mur_forward() marks a non-BROKEN process vector as ACTIVE_PROC when it finds it.
 * mur_forward() call to this routine does not change state of BROKEN_PROC
 * When mur_forward() finds correspinding PFIN record, it marks it FINISHED.
 */
uint4		mur_pini_state(jnl_ctl_list *jctl, uint4 pini_addr, int state)
{
	pini_list_struct	*plst;
	uint4			status;

	status = mur_get_pini(jctl, pini_addr, &plst);
	if (SS_NORMAL != status)
		return status;
	if (BROKEN_PROC == plst->state)
		return SS_NORMAL;
	plst->state = (enum pini_rec_stat)state;
	return SS_NORMAL;
}
