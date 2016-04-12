/****************************************************************
 *								*
 *	Copyright 2004, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"		/* needed for gdsfhead.h */
#include "gdsblk.h"		/* needed for gdsfhead.h */
#include "gtm_facility.h"	/* needed for gdsfhead.h */
#include "fileinfo.h"		/* needed for gdsfhead.h */
#include "gdsbt.h"		/* needed for gdsfhead.h */
#include "gdsfhead.h"		/* needed for gvcst_protos.h */
#include "gdsblkops.h"
#include "filestruct.h"
#include "jnl.h"
#include "gdscc.h"		/* needed for tp.h */
#include "gdskill.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"

#include "mvalconv.h"		/* for i2mval prototype for the MV_FORCE_MVAL macro */
#include "gvcst_protos.h"	/* for gvcst_incr prototype */

GBLREF	boolean_t	in_gvcst_incr;
GBLREF	mval		increment_delta_mval;
GBLREF	mval		*post_incr_mval;
GBLREF	sgm_info	*sgm_info_ptr;

void	gvcst_incr(mval *increment, mval *result)
{
	assert(!in_gvcst_incr);
	assert(MV_IS_NUMERIC(increment));	/* op_gvincr or gtcmtr_increment should have done the MV_FORCE_NUM before calling */
	in_gvcst_incr = TRUE;
	post_incr_mval = result;
	/* it is possible (due to some optimizations in the compiler) that both the input parameters "increment" and "result"
	 * point to the same mval. if we pass "increment" and "result" as they are to gvcst_put, it is possible due to the code
	 * flow that gvcst_put needs to read the value of "increment" after it is done modifying "result" (it can do this by
	 * changing the global variable "post_incr_mval"). in this case it will read a bad value of "increment" (since it will
	 * now be reading the modified value of "result"). therefore we do not pass them as they are. Instead, since we are
	 * interested in only the numeric value of "increment", we take a copy of "increment" into an mval ("increment_delta_mval")
	 * and force it to be numeric. It is the address of this mval that is passed to gvcst_put. Any changes to "post_incr_mval"
	 * will not affect this mval so it is safe to read this anytime in gvcst_put. The mval "increment_delta_mval" is also
	 * used in gvincr_recompute_upd_array.
	 */
	increment_delta_mval = *increment;
	/* Since we should be caring about just the numeric part, nullify the string part of the mval */
	increment_delta_mval.str.len = 0;
	gvcst_put(&increment_delta_mval);
	assert(!in_gvcst_incr);	/* should have been reset by gvcst_put */
	in_gvcst_incr = FALSE;	/* just in case it is not reset already by gvcst_put */
}
