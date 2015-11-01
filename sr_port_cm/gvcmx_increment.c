/****************************************************************
 *								*
 *	Copyright 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "cmidef.h"
#include "hashtab_mname.h"	/* needed for cmmdef.h */
#include "cmmdef.h"
#include "gvcmx.h"
#include "gvcmz.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

#include "format_targ_key.h"	/* for format_targ_key prototype */

GBLREF	gd_region	*gv_cur_region;
GBLREF	gv_key		*gv_currkey;

/* returns FALSE if gv_currkey is undefined in the server end and undef_inhibit is turned OFF */
void	gvcmx_increment(mval *increment, mval *result)
{
	unsigned char	buff[MAX_ZWR_KEY_SZ], *end;
	mval		tmpmval;

	error_def(ERR_UNIMPLOP);
	error_def(ERR_TEXT);
	error_def(ERR_GVIS);

	if (!((link_info *)gv_cur_region->dyn.addr->cm_blk->usr)->server_supports_dollar_incr)
	{
		assert(dba_cm == gv_cur_region->dyn.addr->acc_meth); /* we should've covered all other access methods elsewhere */
		end = format_targ_key(buff, MAX_ZWR_KEY_SZ, gv_currkey, TRUE);
		rts_error(VARLSTCNT(14) ERR_UNIMPLOP, 0,
					ERR_TEXT, 2, LEN_AND_LIT("GT.CM server does not support $INCREMENT operation"),
					ERR_GVIS, 2, end - buff, buff,
					ERR_TEXT, 2, REG_LEN_STR(gv_cur_region));
	}
	/* gvcmz_doop() currently accepts only one argument.
	 * It serves as an input argument for SET.
	 * It serves as an output argument for GET etc.
	 * $INCR is unique in that it needs to pass the increment as input and expects the post-increment as output.
	 *
	 * In order to accomplish this without changing the gvcmz_doop() interface, we overload the one argument to
	 *	serve two purposes. It will be an input argument until the send of the message to the server and will
	 *	then serve as an output argument after the response from the server. ("result" is used for this purpose)
	 * i.e.
	 *	to serve as increment            for client --> server message
	 *	to serve as post-increment value for server --> client message
	 */
	assert(MV_IS_NUMERIC(increment));	/* op_gvincr would have forced it to be a NUMERIC */
	MV_FORCE_STR(increment);		/* convert it to a string before sending it to gvcmz_doop */
	*result = *increment;
	gvcmz_doop(CMMS_Q_INCREMENT, CMMS_R_INCREMENT, result);
}
