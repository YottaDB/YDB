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
#include "gtm_limits.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "cli.h"
#include "mupip_exit.h"
#include "str_match.h"
#include "mu_getlst.h"
#include "gtmmsg.h"
#include "mupip_dump_fhead.h"
#include "gtm_stdlib.h"

GBLREF char			gtm_dist[GTM_PATH_MAX];
GBLREF boolean_t		gtm_dist_ok_to_use;
GBLREF tp_region		*grlist;

error_def(ERR_DBNOREGION);
error_def(ERR_GTMDISTUNVERIF);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);

#define DUMPFHEAD_CMD_STRING_SIZE 	256 + GTM_PATH_MAX + GTM_PATH_MAX
#define EXEC_GTMDUMPFHEAD		"%s/mumps -run %%XCMD 'do dumpfhead^%%DUMPFHEAD(\"%s\")'"

int4 dumpfhead(int len, unsigned char *filepath);

void mupip_dump_fhead(void)
{
	int4		status;
	tp_region	*rptr;
	unsigned char	file[GTM_PATH_MAX + 1];
	unsigned short	file_len = SIZEOF(file);
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify gtm_dist, and make sure there is a parameter. */
	if (!gtm_dist_ok_to_use)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_GTMDISTUNVERIF, 4, LEN_AND_STR(gtm_dist));
	if (CLI_PRESENT == cli_present("REGION"))
	{	/* region */
		status = SS_NORMAL;
		gvinit();
		mu_getlst("WHAT", SIZEOF(tp_region));
		if (!grlist)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_DBNOREGION);
		for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
		{
			util_out_print("Fileheader dump of region !AD", TRUE, REG_LEN_STR(rptr->reg));
			util_out_print("Dumping fileheader of !AD", TRUE, DB_LEN_STR(rptr->reg));
			status |= dumpfhead(DB_LEN_STR(rptr->reg));
		}
		mupip_exit(status == SS_NORMAL ? status : ERR_MUNOFINISH);
	} else
	{	/* we default to file if neither -file nor -region could be found */
		if (!cli_get_str("WHAT", (char *) file, (unsigned short *) &file_len))
			mupip_exit(ERR_MUPCLIERR);
		file[file_len] = '\0';     /* Null terminate */
		mupip_exit(dumpfhead(file_len, file));
	}
}

int4 dumpfhead(int len, unsigned char *file)
{
	char	cmd_dmpfhead_string[DUMPFHEAD_CMD_STRING_SIZE];
#	ifdef _BSD
	union wait		wait_stat;
#	else
	int4			wait_stat;
#	endif

	SNPRINTF(cmd_dmpfhead_string, SIZEOF(cmd_dmpfhead_string), EXEC_GTMDUMPFHEAD,
		 gtm_dist, file);

#ifdef _BSD
	assert(SIZEOF(wait_stat) == SIZEOF(int4));
	wait_stat.w_status = SYSTEM(cmd_dmpfhead_string);
#else
	wait_stat = SYSTEM(cmd_dmpfhead_string);
#endif
	return WEXITSTATUS(wait_stat);
}
