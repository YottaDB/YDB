/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include "rtnhdr.h"
#include "srcline.h"
#include "error.h"
#include "op.h"
#include "outofband.h"


#define	INFO_MSK(error)	(error & ~SEV_MSK | INFO)

GBLREF int4		outofband;
GBLREF mident_fixed	zlink_mname;

void op_zprint(mval *rtn, mval *start_label, int start_int_exp, mval *end_label, int end_int_exp)
/* contains label to be located or null string		*/
/* contains label offset or line number to reference	*/
/* contains routine to look in or null string		*/
/* NOTE: If only the first label is specified, the 	*/
/*	 parser makes the second label the duplicate	*/
/*	 of the first. (not so vice versa)		*/
{
	mval	print_line, null_str;
	mstr	*src1, *src2;
	uint4	stat1, stat2;
	rhdtyp	*rtn_vector;
	error_def(ERR_FILENOTFND);
	error_def(ERR_TXTSRCMAT);
	error_def(ERR_ZPRTLABNOTFND);
	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLMODULE);

	MV_FORCE_STR(start_label);
	MV_FORCE_STR(end_label);
	MV_FORCE_STR(rtn);
	if (NULL == (rtn_vector = find_rtn_hdr(&rtn->str)))
	{
		op_zlink(rtn, NULL);
		rtn_vector = find_rtn_hdr(&rtn->str);
		if (NULL == rtn_vector)
			rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->str.len, rtn->str.addr,
					ERR_ZLMODULE, 2, mid_len(&zlink_mname), &zlink_mname.c[0]);
	}
	stat1 = get_src_line(rtn, start_label, start_int_exp, &src1);
	if (stat1 & LABELNOTFOUND)
		rts_error(VARLSTCNT(1) ERR_ZPRTLABNOTFND);
	if (stat1 & SRCNOTFND)
		rts_error(VARLSTCNT(4) ERR_FILENOTFND, 2, rtn_vector->src_full_name.len, rtn_vector->src_full_name.addr);
	if (stat1 & (SRCNOTAVAIL | AFTERLASTLINE))
		return;
	if (stat1 & (ZEROLINE | NEGATIVELINE))
	{
		null_str.mvtype = MV_STR;
		null_str.str.len = 0;
		stat1 = get_src_line(rtn, &null_str, 1, &src1);
		if (stat1 & AFTERLASTLINE)		/* the "null" file */
			return;
	}
	if (end_int_exp == 0 && (end_label->str.len == 0 || *end_label->str.addr == 0))
		stat2 = AFTERLASTLINE;
	else if ((stat2 = get_src_line(rtn, end_label, end_int_exp, &src2)) & LABELNOTFOUND)
		rts_error(VARLSTCNT(1) ERR_ZPRTLABNOTFND);
	if (stat2 & (ZEROLINE | NEGATIVELINE))
		return;
	if (stat2 & AFTERLASTLINE)
	{
		null_str.mvtype = MV_STR;
		null_str.str.len = 0;
		stat2 = get_src_line(rtn, &null_str, 1, &src2);
		/* number of lines less one for duplicated zero'th line and one due
		   to termination condition being <=
		*/
		assert((INTPTR_T)src2 > 0);
		src2 += rtn_vector->lnrtab_len - 2;
	}
	if (stat1 & CHECKSUMFAIL)
	{
		rts_error(VARLSTCNT(1) INFO_MSK(ERR_TXTSRCMAT));
		op_wteol(1);
	}
	print_line.mvtype = MV_STR;
	for ( ; src1 <= src2 ; src1++)
	{
		if (outofband)
			outofband_action(FALSE);
		print_line.str.addr = src1->addr;
		print_line.str.len = src1->len;
		op_write(&print_line);
		op_wteol(1);
	}
	return;
}
