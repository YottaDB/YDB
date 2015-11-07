/****************************************************************
 *								*
 *	Copyright 2012, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "lv_val.h"
#include "toktyp.h"
#include "compiler.h"
#include "opcode.h"
#include "indir_enum.h"
#include "cache.h"
#include "op.h"
#include <rtnhdr.h>
#include "valid_mname.h"
#include "gtm_string.h"
#include "cachectl.h"
#include "gtm_text_alloc.h"
#include "callg.h"
#include "mdq.h"
#include "stack_frame.h"
#include "stringpool.h"
#include "mv_stent.h"
#include "min_max.h"
#include "glvn_pool.h"

/* [Used by SET] Store a value in a saved local or global variable. */
void op_stoglvn(uint4 indx, mval *value)
{
	lv_val		*lv;
	opctype		oc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_DEFINED(value);	/* do what op_sto.s does */
	oc = (TREF(glvn_pool_ptr))->slot[indx].sav_opcode;
	if (OC_SAVLVN == oc)
	{	/* lvn */
		lv = op_rfrshlvn(indx, OC_PUTINDX);
		/* Below two lines do what op_sto.s does. Ideally we should be invoking op_sto from here but
		 * it is not easily possible due to op_sto expecting its arguments in registers which this
		 * C function cannot ensure. Any changes to op_sto.s might need to be correspondingly made here.
		 */
		lv->v = *value;
		lv->v.mvtype &= ~MV_ALIASCONT;	/* Make sure alias container property does not pass */
	} else if (OC_NOOP != oc)		/* if indirect error blew set up, skip this */
	{	/* gvn */
		op_rfrshgvn(indx, oc);
		op_gvput(value);
	}
}
