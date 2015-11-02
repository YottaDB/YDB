/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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


void db_csh_ini(sgmnt_addrs *cs)
{
	if ((INTPTR_T)cs->hdr & 7)
		GTMASSERT;
	cs->acc_meth.bg.cache_state  = (cache_que_heads_ptr_t)((sm_uc_ptr_t)cs->hdr + cs->nl->cache_off);
	assert(cs->acc_meth.bg.cache_state);
	return;
}
