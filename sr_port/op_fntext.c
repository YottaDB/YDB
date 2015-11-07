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

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "error.h"
#include <rtnhdr.h>
#include "srcline.h"
#include "op.h"
#include "stringpool.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#endif

GBLREF spdesc stringpool;

error_def(ERR_TXTSRCMAT);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

void op_fntext(mval *label, int int_exp, mval *rtn, mval *ret)
/* label contains label to be located or null string */
/* int_exp contains label offset or line number to reference */
/* ret is used to return the correct string to caller */
{
	char		*cp;
	int		i, lbl, letter;
	mval		*temp_rtn, temp_mval;
	mstr		*sld;
	uint4		stat;
	rhdtyp		*rtn_vector;
	boolean_t	is_trigger;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(label);
	MV_FORCE_STR(rtn);
	temp_rtn = &temp_mval;
	*temp_rtn = *rtn;	/* make a copy of the routine in case the caller used the same mval for rtn and ret */
	ret->str.len = 0;	/* make ret an emptystring in case the return is by way of the condition handler */
	ret->mvtype = MV_STR;
	sld = (mstr *)NULL;
	ESTABLISH(fntext_ch);	/* to swallow errors and permit an emptystring result */
	GTMTRIG_ONLY(IS_TRIGGER_RTN(&temp_rtn->str, is_trigger));
	if ((0 == int_exp) && ((0 == label->str.len) || (0 == *label->str.addr)))
		stat = ZEROLINE;
	else
	{
#ifdef GTM_TRIGGER
		if (is_trigger)
		{
			DBGTRIGR((stderr, "op_fntext: fetching $TEXT() source for a trigger\n"));
			assert(FALSE == TREF(in_op_fntext));
			TREF(in_op_fntext) = TRUE;
		}
#endif
		stat = get_src_line(temp_rtn, label, int_exp, &sld, VERIFY);
		GTMTRIG_ONLY(TREF(in_op_fntext) = FALSE);
	}
	if (0 == (stat & (CHECKSUMFAIL | NEGATIVELINE)))
	{
		if (stat & ZEROLINE)
		{
			if (NULL == (rtn_vector = find_rtn_hdr(&temp_rtn->str)))
			{	/* not here, so try to bring it in */
				GTMTRIG_ONLY(if (!is_trigger))	/* Triggers cannot be loaded in this fashion */
				{
					op_zlink(temp_rtn, 0);
					rtn_vector = find_rtn_hdr(&temp_rtn->str);
				}
			}
			if (NULL != rtn_vector)
			{
				ret->str.addr = rtn_vector->routine_name.addr;
				ret->str.len = rtn_vector->routine_name.len;
			}
		} else  if (NULL != sld)
			ret->str = *sld;
	}
	REVERT;
	/* If non-empty, copy result to stringpool and
	 * convert any tabs in linestart to spaces
	 */
	if (ret->str.len)
	{
		ENSURE_STP_FREE_SPACE(ret->str.len);
		cp = (char *)stringpool.free;
		for (i = 0, lbl = 1; i < ret->str.len; i++)
		{
			letter = ret->str.addr[i];
			if (lbl)
			{
				if ((' ' == letter) || ('\t' == letter))
				{
					letter = ' ';
					lbl = 0;
				}
				*cp++ = letter;
			} else
			{
				if ((' ' != letter) && ('\t' != letter))
				{
					memcpy(cp, &ret->str.addr[i], ret->str.len - i);
					break;
				} else
					*cp++ = ' ';
			}
		}
		ret->str.addr = (char *)stringpool.free;
		stringpool.free += ret->str.len;
	}
	return;
}
