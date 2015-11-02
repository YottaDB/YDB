/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include "mvalconv.h"
#include "getjobnum.h"
#include "getjobname.h"

GBLREF uint4 process_id;
GBLDEF mval dollar_job;
static char djbuff[10];	/* storage for dollar job's string form */

void getjobname(void)
{
	getjobnum();
	i2usmval(&dollar_job, process_id);
	n2s(&dollar_job);
	assert(dollar_job.str.len <= SIZEOF(djbuff));
	memcpy(djbuff,dollar_job.str.addr,dollar_job.str.len);
	dollar_job.str.addr = djbuff;
}
