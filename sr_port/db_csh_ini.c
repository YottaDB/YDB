/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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


void db_csh_ini(sgmnt_addrs *csa)
{
	assertpro(0 == ((INTPTR_T)csa->hdr & 7));
	csa->acc_meth.bg.cache_state  = (cache_que_heads_ptr_t)((sm_uc_ptr_t)csa->hdr + csa->nl->cache_off);
	assert(csa->acc_meth.bg.cache_state);
	return;
}
