/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#include "compiler.h"
#include "min_max.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#else
# define DBGIFTRIGR(x)
# define DBGTRIGR(x)
# define DBGTRIGR_ONLY(x)
#endif
#include "stack_frame.h"

GBLREF	spdesc		stringpool;
GBLREF	stack_frame	*frame_pointer;
GTMTRIG_ONLY(GBLREF	uint4		dollar_tlevel);
DBGTRIGR_ONLY(GBLREF	unsigned int	t_tries;)

error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

void op_fntext(mval *label, int int_exp, mval *rtn, mval *ret)
/* label contains label to be located or null string */
/* int_exp contains label offset or line number to reference */
/* ret is used to return the correct string to caller */
{
	char			*cp;
	int			i, lbl, letter;
	mval			*temp_rtn, temp_mval;
	mstr			*sld;
	uint4			stat;
	rhdtyp			*rtn_vector;
	boolean_t		current_rtn = FALSE;
	GTMTRIG_ONLY(boolean_t	is_trigger;)
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(label);
	MV_FORCE_STR(rtn);
	temp_rtn = &temp_mval;
	*temp_rtn = *rtn;	/* make a copy of the routine in case the caller used the same mval for rtn and ret */
	if (WANT_CURRENT_RTN(temp_rtn)) /* we want $TEXT for the routine currently executing. */
		current_rtn = TRUE;
	ret->str.len = 0;	/* make ret an emptystring in case the return is by way of the condition handler */
	ret->mvtype = MV_STR;
	sld = (mstr *)NULL;
	ESTABLISH(fntext_ch);	/* to swallow errors and permit an emptystring result */
	GTMTRIG_ONLY(IS_TRIGGER_RTN(&temp_rtn->str, is_trigger));
	DBGIFTRIGR((stderr, "op_fntext: entering $tlevel=%d $t_tries=%d\n", dollar_tlevel, t_tries));
	if ((0 == int_exp) && ((0 == label->str.len) || (0 == *label->str.addr)))
		stat = ZEROLINE;
	else
	{
#ifdef GTM_TRIGGER
		DBGIFTRIGR((stderr, "op_fntext: fetching $TEXT() source for a trigger\n"));
		if (is_trigger)
		{
			assert(0 == TREF(op_fntext_tlevel));
			TREF(op_fntext_tlevel) = 1 + dollar_tlevel;
		}
#endif
		stat = get_src_line(temp_rtn, label, int_exp, &sld, &rtn_vector);
		GTMTRIG_ONLY(TREF(op_fntext_tlevel) = 0);
	}
	if (0 == (stat & (CHECKSUMFAIL | NEGATIVELINE)))
	{
		if (stat & ZEROLINE)
		{
			if (current_rtn)
				rtn_vector = frame_pointer->rvector;
			else
			{
				rtn_vector = find_rtn_hdr(&temp_rtn->str);
				if ((NULL == rtn_vector) GTMTRIG_ONLY(&& !is_trigger))
				{	/* not here, so try to bring it in... Triggers cannot be loaded in this fashion */
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
	DBGIFTRIGR((stderr, "op_fntext: exiting\n\n"));
	return;
}
