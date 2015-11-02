/****************************************************************
 *								*
 *	Copyright 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* General repository for global variable definitions used both in DDPGVUSR and GTMSHR. gvusr_init should setup these globals */
/* for use in DDPGVUSR.*/

/***************** DO NOT MOVE THESE GLOBALS INTO GBLDEFS.C; IT WILL CAUSE DDPGVUSR.EXE BLOAT *****************/

#include "mdef.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

GBLDEF	gd_region	*gv_cur_region;
GBLDEF	gv_key		*gv_altkey, *gv_currkey;
GBLDEF	bool		caller_id_flag = TRUE;

#ifdef INT8_SUPPORTED
	GBLDEF	const seq_num	seq_num_zero = 0;
	GBLDEF	const seq_num	seq_num_one = 1;
	GBLDEF	const seq_num	seq_num_minus_one = (seq_num)-1;
#else
	GBLDEF	const seq_num	seq_num_zero = {0, 0};
	GBLDEF	const seq_num	seq_num_minus_one = {(uint4)-1, (uint4)-1};
#	ifdef BIGENDIAN
		GBLDEF	const seq_num	seq_num_one = {0, 1};
#	else
		GBLDEF	const seq_num	seq_num_one = {1, 0};
#	endif
#endif
