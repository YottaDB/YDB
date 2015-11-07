/****************************************************************
 *								*
 * Copyright (c) 2010-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>
#include "gtm_string.h"

#include "gtm_limits.h"
#include "error.h"
#include "jobexam_process.h"
#include "gtmdbglvl.h"
#include "create_fatal_error_zshow_dmp.h"

GBLREF	int4		exi_condition;
GBLREF	uint4		gtmDebugLevel;
GBLREF	volatile int4	gtmMallocDepth;
GBLREF	uint4		process_id;
GBLREF	int		process_exiting;

/* Create GTM_FATAL_ERROR* ZSHOW dump file for given fatal condition */
void create_fatal_error_zshow_dmp(int4 signal)
{
	unsigned char	dump_fn[GTM_PATH_MAX], *dump_fn_ptr;
	mval		dump_fn_mval, dummy_mval;
	int4		save_SIGNAL;	/* On UNIX this is exi_condition */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Push out our error now before we potentially overlay it in jobexam_process() */
	PRN_ERROR;
	assert(process_exiting);
	if (0 == gtmMallocDepth && ((SIGBUS != exi_condition && SIGSEGV != exi_condition)
				    || (GDL_ZSHOWDumpOnSignal & gtmDebugLevel)))
	{	/* For this dumpable condition, create a ZSHOW "*" dump for review. First, build the name we
		 * want the report to have.
		 */
		MEMCPY_LIT(dump_fn, GTMFATAL_ERROR_DUMP_FILENAME);
		dump_fn_ptr = dump_fn + (SIZEOF(GTMFATAL_ERROR_DUMP_FILENAME) - 1);
		dump_fn_ptr = i2asc(dump_fn_ptr, process_id);
		*dump_fn_ptr++ = '_';
		/* Use bumped value of jobexam_counter but don't actually increment the counter. The actual increment
		 * is done in jobexam_dump() as part of the default file name (not used here).
		 */
		dump_fn_ptr = i2asc(dump_fn_ptr, (TREF(jobexam_counter) + 1));
		MEMCPY_LIT(dump_fn_ptr, GTMFATAL_ERROR_DUMP_FILETYPE);
		dump_fn_ptr += (SIZEOF(GTMFATAL_ERROR_DUMP_FILETYPE) - 1);
		dump_fn_mval.mvtype = MV_STR;
		dump_fn_mval.str.addr = (char *)dump_fn;
		dump_fn_mval.str.len = INTCAST(dump_fn_ptr - dump_fn);
		assert(GTM_PATH_MAX >= dump_fn_mval.str.len);
		/* Create dump file */
		save_SIGNAL = SIGNAL; 		/* Signal might be modified by jobexam_process() */
		jobexam_process(&dump_fn_mval, &dummy_mval);
		SIGNAL = save_SIGNAL;
	}
}
