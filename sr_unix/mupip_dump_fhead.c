/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "tp.h"
#include "util.h"
#include "cli.h"
#include "mupip_exit.h"
#include "str_match.h"
#include "mu_getlst.h"
#include "gtmmsg.h"
#include "mupip_dump_fhead.h"
#include "gtm_stdlib.h"
#include "wcs_flu.h"
#include "mdq.h"

GBLREF char			ydb_dist[YDB_PATH_MAX];
GBLREF boolean_t		ydb_dist_ok_to_use;
GBLREF tp_region		*grlist;
GBLREF gd_region		*gv_cur_region;
GBLREF sgmnt_addrs		*cs_addrs;
GBLREF usr_reg_que		*usr_spec_regions;

error_def(ERR_BUFFLUFAILED);
error_def(ERR_DBNOREGION);
error_def(ERR_YDBDISTUNVERIF);
error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);

#define DUMPFHEAD_CMD_STRING_SIZE 	256 + YDB_PATH_MAX + YDB_PATH_MAX
#define EXEC_GTMDUMPFHEAD		"%s/mumps -run %%XCMD 'do dumpfhead^%%DUMPFHEAD(\"%s\")'"

int4 dumpfhead(int len, unsigned char *filepath);

void mupip_dump_fhead(void)
{
	int4		status;
	tp_region	*rptr;
	unsigned char	file[YDB_PATH_MAX];
	unsigned short	file_len = YDB_PATH_MAX - 1;
	usr_reg_que	*region_que_entry;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Verify ydb_dist, and make sure there is a parameter. */
	if (!ydb_dist_ok_to_use)
		RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(3) ERR_YDBDISTUNVERIF, 4, LEN_AND_STR(ydb_dist));
	if (CLI_PRESENT == cli_present("REGION"))
	{	/* region */
		status = SS_NORMAL;
		gvinit();
		mu_getlst("REGION", SIZEOF(tp_region));
		if (!grlist)
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_DBNOREGION);
		dqloop(usr_spec_regions, que, region_que_entry)
		{
			for (rptr = grlist; NULL != rptr; rptr = rptr->fPtr)
			{
				gv_cur_region = rptr->reg;
				if ((char *)gv_cur_region->rname == (char *)region_que_entry->usr_reg)
					break; /* Matching region found. Exit the loop */
			}
			if (NULL == rptr)
				continue; /* continue the dqloop */
			if (CLI_PRESENT == cli_present("FLUSH"))
			{
				gv_init_reg(rptr->reg);
				gv_cur_region = rptr->reg; /* required for wcs_flu */
				cs_addrs = &FILE_INFO(gv_cur_region)->s_addrs;
				if (TRUE == grab_crit_immediate(gv_cur_region, TRUE, NOT_APPLICABLE))
				{
					if (!wcs_flu(WCSFLU_FLUSH_HDR))
					{
						rel_crit(gv_cur_region);
						gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(6)
								MAKE_MSG_WARNING(ERR_BUFFLUFAILED), 4,
								LEN_AND_LIT("MUPIP DUMPFHEAD -FLUSH"
								" while flushing file header elements"),
								DB_LEN_STR(gv_cur_region));
					} else
						rel_crit(gv_cur_region);
				} else
				{
					gtm_putmsg_csa(CSA_ARG(REG2CSA(gv_cur_region)) VARLSTCNT(6)
							MAKE_MSG_WARNING(ERR_BUFFLUFAILED), 4,
							LEN_AND_LIT("MUPIP DUMPFHEAD -FLUSH while grabbing critical section"),
							DB_LEN_STR(gv_cur_region));
				}
			}
			util_out_print("Fileheader dump of region !AD", TRUE, REG_LEN_STR(rptr->reg));
			util_out_print("Dumping fileheader of !AD", TRUE, DB_LEN_STR(rptr->reg));
			status |= dumpfhead(DB_LEN_STR(rptr->reg));
		}
	} else
	{	/* we default to file if neither -file nor -region could be found */
		if (!cli_get_str("WHAT", (char *) file, (unsigned short *) &file_len))
			mupip_exit(ERR_MUPCLIERR);
		file[file_len] = '\0';     /* Null terminate */
		status = dumpfhead(file_len, file);
	}
	mupip_exit((SS_NORMAL == status) ? status : ERR_MUNOFINISH);
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
		 ydb_dist, file);

#ifdef _BSD
	assert(SIZEOF(wait_stat) == SIZEOF(int4));
	wait_stat.w_status = SYSTEM(cmd_dmpfhead_string);
#else
	wait_stat = SYSTEM(cmd_dmpfhead_string);
#endif
	return WEXITSTATUS(wait_stat);
}
