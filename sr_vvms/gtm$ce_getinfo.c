/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include <ssdef.h>
#include <descrip.h>

#include "mdef.h"
#include "svnames.h"
#include "gtm_ce.h"
#include "op.h"


GBLREF unsigned short	source_name_len;
GBLREF char		source_file_name[];
GBLREF mident		routine_name;


int4 GTM$CE_GETINFO(uint4 item_code, int4 *resultant_value, struct dsc$descriptor_s resultant_string, uint4 *resultant_length)
{
	mval	v;
	char	*item_start, *item_string;
	int4	copy_len, i, pc, pn;

	error_def(ERR_CENOINDIR);
	error_def(ERR_UNIMPLOP);


	switch ((enum GTM$INFO_CODE)item_code)
	{
	case GTCE$K_SFN:
		copy_len = source_name_len;
		if (copy_len > resultant_string.dsc$w_length)
			copy_len = resultant_string.dsc$w_length;
		memcpy (resultant_string.dsc$a_pointer, source_file_name, copy_len);
		*resultant_length = copy_len;
		break;

	case GTCE$K_RTN:
		copy_len = routine_name.len;
		if (copy_len > resultant_string.dsc$w_length)
			copy_len = resultant_string.dsc$w_length;
		memcpy(resultant_string.dsc$a_pointer, routine_name.addr, copy_len);
		*resultant_length = copy_len;
		break;

	case GTCE$K_INDIR:
		stx_error(ERR_CENOINDIR);
		*resultant_value = 0;
		break;

	case GTCE$K_ZROU:
		op_svget (SV_ZROUTINES, &v);
		copy_len = v.str.len;
		if (copy_len > resultant_string.dsc$w_length)
			copy_len = resultant_string.dsc$w_length;
		memcpy (resultant_string.dsc$a_pointer, v.str.addr, copy_len);
		*resultant_length = copy_len;
		break;

	case GTCE$K_ZCOM:
		op_svget (SV_ZCOMPILE, &v);
		copy_len = v.str.len;
		if (copy_len > resultant_string.dsc$w_length)
			copy_len = resultant_string.dsc$w_length;
		memcpy (resultant_string.dsc$a_pointer, v.str.addr, copy_len);
		*resultant_length = copy_len;
		break;

	case GTCE$K_P1:
	case GTCE$K_P2:
	case GTCE$K_P3:
	case GTCE$K_P4:
	case GTCE$K_P5:
	case GTCE$K_P6:
	case GTCE$K_P7:
	case GTCE$K_P8:
	case GTCE$K_CMDLIN:
		op_svget (SV_ZCMDLINE, &v);

		switch ((enum GTM$INFO_CODE)item_code)
		{
		case GTCE$K_CMDLIN:	pn = 0; break;
		case GTCE$K_P1:		pn = 1; break;
		case GTCE$K_P2:		pn = 2; break;
		case GTCE$K_P3:		pn = 3; break;
		case GTCE$K_P4:		pn = 4; break;
		case GTCE$K_P5:		pn = 5; break;
		case GTCE$K_P6:		pn = 6; break;
		case GTCE$K_P7:		pn = 7; break;
		case GTCE$K_P8:		pn = 8; break;
		}
		item_string = v.str.addr;
		if (pn == 0)
		{
			copy_len = v.str.len;
			item_start = item_string;
		}
		else
		{
			pc = pn - 1;
			i = 0;
			while (pc > 0  &&  i++ < v.str.len  &&  *item_string++)
			{
				if (*item_string == ',')
				{
					pc--;
					item_string++;
					i++;
				}
			}
			item_start = item_string;
			for (copy_len = 0;  *item_string++ != ',' && i++ < v.str.len;  copy_len++) ;
		}

		if (copy_len > resultant_string.dsc$w_length)
			copy_len = resultant_string.dsc$w_length;
		if (copy_len > 0)
			memcpy (resultant_string.dsc$a_pointer, item_start, copy_len);
		*resultant_length = copy_len;
		break;

	default:
		rts_error (VARLSTCNT(1) ERR_UNIMPLOP);
		break;
	}

	return SS$_NORMAL;
}
