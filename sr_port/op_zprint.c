/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
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

#include <rtnhdr.h>
#include "srcline.h"
#include "error.h"
#include "op.h"
#include "outofband.h"
#include "min_max.h"
#ifdef GTM_TRIGGER
# include "gtm_trigger_trc.h"
#else
# define DBGIFTRIGR(x)
# define DBGTRIGR(x)
# define DBGTRIGR_ONLY(x)
#endif

#define	INFO_MSK(error)	(error & ~SEV_MSK | INFO)

DBGTRIGR_ONLY(GBLREF	uint4		dollar_tlevel;)
DBGTRIGR_ONLY(GBLREF	unsigned int	t_tries;)
GBLREF int4		outofband;
GBLREF mident_fixed	zlink_mname;

error_def(ERR_FILENOTFND);
error_def(ERR_TRIGNAMENF);
error_def(ERR_TXTSRCMAT);
error_def(ERR_ZPRTLABNOTFND);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

void op_zprint(mval *rtn, mval *start_label, int start_int_exp, mval *end_label, int end_int_exp)
/* contains label to be located or null string		*/
/* contains label offset or line number to reference	*/
/* contains routine to look in or null string		*/
/* NOTE: If only the first label is specified, the 	*/
/*	 parser makes the second label the duplicate	*/
/*	 of the first. (not so vice versa)		*/
{
	mval			print_line, null_str;
	mstr			*src1, *src2;
	uint4			stat1, stat2;
	rhdtyp			*rtn_vector, *rtn_vector2;
	DBGTRIGR_ONLY(boolean_t	is_trigger;)

	MV_FORCE_STR(start_label);
	MV_FORCE_STR(end_label);
	MV_FORCE_STR(rtn);
	/* Note this initial setting of is_trigger is necessary (even though it gets replaced a few lines down) because
	 * the macro DBGIFTRIGR uses it.
	 */
	DBGTRIGR_ONLY(IS_TRIGGER_RTN(&rtn->str, is_trigger));
	DBGIFTRIGR((stderr, "op_zprint: Performing zprint of a trigger $tlevel=%d $trestart=%d\n", dollar_tlevel, t_tries));
	stat1 = get_src_line(rtn, start_label, start_int_exp, &src1, &rtn_vector);
	DBGTRIGR_ONLY(is_trigger = ((NULL != rtn_vector) && (NULL != rtn_vector->trigr_handle)));
	DBGIFTRIGR((stderr, "op_zprint: get_src_line returned %d for %lx\n", stat1, rtn_vector));
	if (OBJMODMISS == stat1)
	{
#		ifdef GTM_TRIGGER
		if ((NULL != rtn_vector) && (NULL != rtn_vector->trigr_handle))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGNAMENF, 2, rtn->str.len, rtn->str.addr);
#		endif
		/* get_src_line did not find the object file to load */
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->str.len, rtn->str.addr,
			  ERR_ZLMODULE, 2, mid_len(&zlink_mname), &zlink_mname.c[0]);
	}
	/* In case of GTM_TRIGGER, rtn_vector would have been initialized in the call to get_src_line above.
	 * We need to use that and not do a find_rtn_hdr as rtn->str might contain region-name disambiguator
	 * which find_rtn_hdr has no clue about. Even in case of no GTM_TRIGGER, we should be able to use rtn_vector
	 * from "get_src_line". Assert that below before using.
	 */
	DEBUG_ONLY(NON_GTMTRIG_ONLY(rtn_vector2 = find_rtn_hdr(&rtn->str);))
	NON_GTMTRIG_ONLY(assert(rtn_vector2 == rtn_vector);)
	assertpro(NULL != rtn_vector);	/* If couldn't find module, should have returned OBJMODMISS */
	if (stat1 & LABELNOTFOUND)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPRTLABNOTFND);
	if (stat1 & SRCNOTFND)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_FILENOTFND, 2,
				rtn_vector->src_full_name.len, rtn_vector->src_full_name.addr);
	if (stat1 & (SRCNOTAVAIL | AFTERLASTLINE))
		return;
	if (stat1 & (ZEROLINE | NEGATIVELINE))
	{
		null_str.mvtype = MV_STR;
		null_str.str.len = 0;
		stat1 = get_src_line(rtn, &null_str, 1, &src1, NULL);
		if (stat1 & AFTERLASTLINE)		/* the "null" file */
			return;
	}
	if (end_int_exp == 0 && (end_label->str.len == 0 || *end_label->str.addr == 0))
		stat2 = AFTERLASTLINE;
	else if ((stat2 = get_src_line(rtn, end_label, end_int_exp, &src2, NULL)) & LABELNOTFOUND)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_ZPRTLABNOTFND);
	if (stat2 & (ZEROLINE | NEGATIVELINE))
		return;
	if (stat2 & AFTERLASTLINE)
	{
		null_str.mvtype = MV_STR;
		null_str.str.len = 0;
		stat2 = get_src_line(rtn, &null_str, 1, &src2, NULL);
		assert(NULL != src2);
		/* number of lines less one for duplicated zero'th line and one due
		   to termination condition being <=
		*/
		src2 += rtn_vector->lnrtab_len - 2;
	}
	if (stat1 & CHECKSUMFAIL)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) INFO_MSK(ERR_TXTSRCMAT));
		op_wteol(1);
	}
	print_line.mvtype = MV_STR;
	DBGIFTRIGR((stderr, "op_zprint: printing the trigger from 0x%lx to 0x%lx\n", src1, src2));
	for ( ; src1 <= src2 ; src1++)
	{	/* Note outofband check currently disabled. This routine (op_zprint) needs to be rewritten to provide
		 * a TP wrapper (if not already in place) and to buffer the lines obtained from "get_src_line" completely
		 * before outputting anything because obtaining these source lines is subject to TP restarts when we are
		 * accessing triggers. In addition, for the case of "normal" routine source fetches, an outofband could
		 * invoke a job interrupt which could relink an entry point so an out-of-band interrupt of any kind means
		 * the source fetches also need to be restarted. Until these issues are address, the outofband check
		 * remains disabled (SE - 12/2010)
		 */
		/* if (outofband)
		  outofband_action(FALSE); */
		print_line.str.addr = src1->addr;
		print_line.str.len = src1->len;
		op_write(&print_line);
		op_wteol(1);
	}
	DBGIFTRIGR((stderr, "op_zprint: exiting\n\n"));
	return;
}
