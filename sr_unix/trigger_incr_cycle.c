/****************************************************************
 *								*
 *	Copyright 2010, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gdsroot.h"			/* for gdsfhead.h */
#include "gdsbt.h"			/* for gdsfhead.h */
#include "gdsfhead.h"			/* For gvcst_protos.h */
#include "gvcst_protos.h"
#include <rtnhdr.h>			/* for gv_trigger.h */
#include "gv_trigger.h"
#include "trigger.h"
#include "trigger_incr_cycle.h"
#include "gdsblk.h"
#include "mv_stent.h"			/* for COPY_SUBS_TO_GVCURRKEY macro */
#include "gvsub2str.h"			/* for COPY_SUBS_TO_GVCURRKEY */
#include "format_targ_key.h"		/* for COPY_SUBS_TO_GVCURRKEY */
#include "mvalconv.h"			/* Needed for MV_FORCE_* */

GBLREF	gd_region		*gv_cur_region;
GBLREF	gv_key			*gv_currkey;

void trigger_incr_cycle(char *trigvn, int trigvn_len)
{
	uint4			cycle;
	char			*cycle_ptr, cycle_str[MAX_DIGITS_IN_INT + 1];
	char			*ptr, *ptr1;
	int4			result;
	mval			trigger_cycle, *mv_trig_cycle_ptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	BUILD_HASHT_SUB_SUB_CURRKEY(trigvn, trigvn_len, LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE));
	if (gvcst_get(&trigger_cycle))
	{
		mv_trig_cycle_ptr = &trigger_cycle;
		cycle = MV_FORCE_INT(mv_trig_cycle_ptr);
		cycle++;
		INT2STR(cycle, cycle_str);
		cycle_ptr = cycle_str;
	} else
		cycle_ptr = INITIAL_CYCLE;
	SET_TRIGGER_GLOBAL_SUB_SUB_STR(trigvn, trigvn_len, LITERAL_HASHCYCLE, STRLEN(LITERAL_HASHCYCLE),
		cycle_ptr, STRLEN(cycle_ptr), result);
	assert(PUT_SUCCESS == result);
}
