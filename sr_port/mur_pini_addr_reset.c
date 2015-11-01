/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
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
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab.h"
#include "muprec.h"

GBLREF	jnl_ctl_list		*mur_jctl;

/* this routine resets new_pini_addr to 0 for all process-vectors in the current jctl's (mur_jctl) hash-table entries.
 * this is usually invoked in case a journal auto switch occurs while backward recover/rollback is playing forward the updates
 */
void	mur_pini_addr_reset(void)
{
	pini_list_struct	*plst;
	ht_ent_int4 		*tabent, *topent;

	for (tabent = mur_jctl->pini_list.base, topent = mur_jctl->pini_list.top; tabent < topent; tabent++)
	{
		if (HTENT_VALID_INT4(tabent, pini_list_struct, plst))
			plst->new_pini_addr = 0;
	}
}
