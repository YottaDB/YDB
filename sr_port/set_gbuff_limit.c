/****************************************************************
 *								*
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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
#include "mvalconv.h"
#include "min_max.h"
#include "set_gbuff_limit.h"

/* sets the value of gbuff_limit (in struct sgmnt_addrs), depending on the value of the poollimit settings
 * (which in turn is set using environment var "gtm_poollimit", and has a default value in case of MUPIP REORG.
 */
void set_gbuff_limit(sgmnt_addrs **csaptr, sgmnt_data_ptr_t *csdptr, mval *poollimit_arg)
{
	int4			nbuffs;
	sgmnt_addrs		*csa;
	sgmnt_data_ptr_t	csd;
	cache_rec_ptr_t		cr;

	csa = *csaptr;
	csd = *csdptr;
	nbuffs = ((dba_bg == csd->acc_meth) && (NULL != poollimit_arg)) ? MV_FORCE_INT(poollimit_arg) : 0;
	if (nbuffs && (MV_STR & poollimit_arg->mvtype) && ('%' == poollimit_arg->str.addr[poollimit_arg->str.len - 1]))
		nbuffs = (100 == nbuffs) ? 0 : (csd->n_bts * nbuffs) / 100;		/* Percentage */
	csa->gbuff_limit = (0 == nbuffs) ? 0 : MAX(MIN(nbuffs, csd->n_bts * .5), MIN_GBUFF_LIMIT);
	/* To pick the current "hand" as a pseudo-random spot for our area see dbg code in gvcst_init
	 * but for the first release of this always pick the end of the buffer
	 */
	csa->our_midnite = csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets + csd->n_bts;
	cr = csa->our_midnite - csa->gbuff_limit;
	if (cr < csa->acc_meth.bg.cache_state->cache_array + csd->bt_buckets)
		cr += csd->n_bts;
	csa->our_lru_cache_rec_off = GDS_ANY_ABS2REL(csa, cr);
}
