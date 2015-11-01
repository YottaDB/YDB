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

#include "error.h"
#include "rtnhdr.h"
#include "srcline.h"
#include "op.h"

GBLREF mident zlink_mname;

void op_fntext(mval *label, int int_exp, mval *rtn, mval *ret)
/* label contains label to be located or null string */
/* int_exp contains label offset or line number to reference */
/* ret is used to return the correct string to caller */
{
	char		*cp, *ctop;
	mval		*temp_rtn, temp_mval;
	src_lin_dsc	*sld;
	uint4		stat;
	rhdtyp		*rtn_vector;

	error_def(ERR_TXTNEGLIN);
	error_def(ERR_TXTSRCMAT);
	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLMODULE);

	MV_FORCE_STR(label);
	MV_FORCE_STR(rtn);
	temp_rtn = &temp_mval;
	*temp_rtn = *rtn;	/* make a copy of the routine in case the caller used the same mval for rtn and ret */
	ret->str.len = 0;	/* make ret an emptystring in case the return is by way of the condition handler */
	ret->mvtype = MV_STR;
	sld = (src_lin_dsc *)NULL;
	ESTABLISH(fntext_ch);	/* to swallow errors and permit an emptystring result */
	if ((int_exp == 0) && ((label->str.len == 0) || (*label->str.addr == 0)))
		stat = ZEROLINE;
	else
		stat = get_src_line(temp_rtn, label, int_exp, &sld);
	if ((FALSE == (stat & CHECKSUMFAIL)) && (FALSE == (stat & NEGATIVELINE)))
	{
		if (stat & ZEROLINE)
		{
			if (NULL == (rtn_vector = find_rtn_hdr(&temp_rtn->str)))
			{	/* not here, so try to bring it in */
				op_zlink(temp_rtn, 0);
				rtn_vector = find_rtn_hdr(&temp_rtn->str);
			}
			if (NULL != rtn_vector)
			{
				ret->str.addr = cp = (char *)&rtn_vector->routine_name;
				for (ctop = cp + sizeof(mident);  *cp && cp < ctop;  cp++)
					;
				ret->str.len = cp - ret->str.addr;
			}
		} else  if (NULL != sld)
			ret->str = sld->text_form;
	}
	REVERT;
	return;
}
