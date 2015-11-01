/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/


#include "mdef.h"

#include "gtm_stdio.h"

#include "compiler.h"
#include "opcode.h"

#define	TEMP_TVAL_PREFIX_NAME	"#T%"

GBLREF triple *curr_fetch_trip, *curr_fetch_opr;
GBLREF int4 curr_fetch_count;

static mvar 	*var;
static int 	temp_val_index=0;

int put_tval(oprtype *ref, short index)
{
        triple 	*fetch;
	char	temp_tval[20];


	if (index == -1)
	{
		index = temp_val_index++;
		SPRINTF(temp_tval,"%s%d",TEMP_TVAL_PREFIX_NAME,index);
        	var = get_mvaddr((mident *)temp_tval);
		index = var->mvidx;

		/* Make sure line fetch fetches the variable */
		fetch = newtriple(OC_PARAMETER);
		curr_fetch_opr->operand[1] = put_tref(fetch);
		fetch->operand[0] = put_ilit(var->mvidx);
		curr_fetch_count++;
		curr_fetch_opr = fetch;
		var->last_fetch = curr_fetch_trip;
	}

        ref->oprclass = TVAR_REF;
        return (ref->oprval.temp = var->mvidx);

}
