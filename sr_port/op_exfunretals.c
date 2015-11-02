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

#include <rtnhdr.h>
#include "stack_frame.h"
#include "op.h"
#include "hashtab_mname.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "alias.h"

GBLREF	mval	*alias_retarg;

error_def(ERR_QUITARGREQD);
error_def(ERR_ALIASEXPECTED);

/* Routine to:
 *   (1) To turn off the MV_RETARG flag in the mval returned from the M function call.
 *   (2) To verify the flag was ON in the first place, else this is not a return value.
 *   (3) To verify the MV_ALIASCONT flag IS on which would signify a QUIT * was done.
 *       To not do this constitutes an error and means QUIT * was NOT done as is
 *       required to create an alias on the caller side.
 */
void op_exfunretals(mval *retval)
{
	unsigned short	savtyp;

	savtyp = retval->mvtype;
	retval->mvtype &= ~MV_RETARG;
	if (0 == (MV_RETARG & savtyp))
		rts_error(VARLSTCNT(1) ERR_QUITARGREQD);
	if (0 == (MV_ALIASCONT & savtyp))
		rts_error(VARLSTCNT(1) ERR_ALIASEXPECTED);
	assert(NULL != alias_retarg);
}

