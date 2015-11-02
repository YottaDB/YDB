/****************************************************************
 *								*
 *	Copyright 2010, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "lv_val.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF	mval		*alias_retarg;
GBLREF	boolean_t	dollar_zquit_anyway;

LITREF	mval 		literal_null;

error_def(ERR_QUITARGREQD);
error_def(ERR_QUITALSINV);

/* Routine to:
 *   (1) To turn off the MV_RETARG flag in the mval returned from the M function call.
 *   (2) To verify the flag was ON in the first place, else this is not a return value.
 *   (3) To verify the MV_ALIASCONT flag is NOT on which would signify a QUIT * was done
 *       which does not match with this non-alias invocation. It would be possible to
 *       convert the arg at this point and let it pass but the requestors of this
 *       enhancement have asked an error to be generated so simple mistakes are not
 *	 difficult to track down. This behavior could easily be made optional at some
 *	 point in the future.
 */
void op_exfunret(mval *retval)
{
	unsigned short	savtyp;
	lv_val		*srclvc;

	savtyp = retval->mvtype;
	retval->mvtype &= ~MV_RETARG;
	if (0 == (MV_RETARG & savtyp))
		/* if dollar_zquit_anyway is TRUE, then do not give an error when no value is returned from an
		 * extrinsic; just make the return value NULL instead
		 */
		if (dollar_zquit_anyway)
			*retval = literal_null;
		else
			rts_error(VARLSTCNT(1) ERR_QUITARGREQD);
	if (0 != (MV_ALIASCONT & savtyp))
	{	/* We have an alias container return which has already had its reference counts increased. Remove
		 * the extra reference counts before we raise the actual error since the assignment and thus the
		 * alias reference are NOT being created.
		 */
		srclvc = (lv_val *)retval->str.addr;
		assert(LV_IS_BASE_VAR(srclvc));		/* Verify base var */
		assert(srclvc->stats.trefcnt >= srclvc->stats.crefcnt);
		assert(1 <= srclvc->stats.crefcnt);	/* Verify is existing container ref */
		DECR_CREFCNT(srclvc);
		DECR_TREFCNT(srclvc);
		alias_retarg = NULL;
		rts_error(VARLSTCNT(1) ERR_QUITALSINV);
	}
	assert(NULL == alias_retarg);
}

