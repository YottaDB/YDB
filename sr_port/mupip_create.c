/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
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
#include "gtm_ctype.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_create.h"
#include "mu_cre_file.h"
#include "gtmmsg.h"

GBLREF gd_addr 		*gd_header;
GBLREF gd_region 	*gv_cur_region;
GBLREF enum db_ver	gtm_db_create_ver;              /* database creation version */

error_def(ERR_DBNOCRE);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOREGION);

void mupip_create(void)
{
	boolean_t	found;
	char		buff[MAX_RN_LEN + 1], create_stat, exit_stat;
	gd_region	*reg, *reg_top;
	int		i;
	unsigned short	reglen;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	exit_stat = EXIT_NRM;
	TREF(ok_to_see_statsdb_regs) = TRUE;
	gvinit();
	/* WARNING: CLI overrides env var */
	if (CLI_PRESENT == cli_present("V6"))
		gtm_db_create_ver = GDSV6;
	else if (CLI_NEGATED == cli_present("V6"))
		gtm_db_create_ver = GDSVCURR;
	if (CLI_PRESENT == cli_present("REGION"))
	{
		reglen = SIZEOF(buff);
	 	if (0 == cli_get_str("REGION", buff, &reglen))
			mupip_exit(ERR_MUPCLIERR);
	 	for (i=0; i < reglen; i++)
	 		buff[i] = TOUPPER(buff[i]); /* ensure uppercase to match gde conventions */
	 	for ( ; ARRAYSIZE(buff) > i; i++)
			buff[i] = 0;
		found = FALSE;
		for (reg = gd_header->regions, reg_top = reg + gd_header->n_regions; reg < reg_top; reg++)
		{
		 	if (0 == memcmp(reg->rname, buff, MAX_RN_LEN))
		 	{
				found = TRUE;
				break;
		 	}
		}
		if (FALSE == found)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, reglen, buff);
			mupip_exit(ERR_MUPCLIERR);
		}
		/* Note that if an explicit region has been specified, we create db even if it is an AUTODB region.
		 * We are also guaranteed the region is not a STATS region because the region name has been uppercased
		 * above and STATS region names are all lower case. This is also asserted below.
		 */
		assert(!IS_STATSDB_REG(reg));
		gv_cur_region = reg;
		create_stat = mu_cre_file(CALLER_IS_MUPIP_CREATE_TRUE);
		exit_stat |= create_stat;
	} else
	{
		for (gv_cur_region = gd_header->regions, reg_top = gv_cur_region + gd_header->n_regions;
			gv_cur_region < reg_top; gv_cur_region++)
		{
			if (IS_AUTODB_REG(gv_cur_region))
				continue;		/* Ignore MUPIP CREATE for auto-created (which includes STATS) DBs */
			create_stat = mu_cre_file(CALLER_IS_MUPIP_CREATE_TRUE);
			exit_stat |= create_stat;
	       	}
		gv_cur_region = NULL;
	}
	if (exit_stat & EXIT_MASK)
		mupip_exit((ERR_DBNOCRE & ~EXIT_MASK) | (exit_stat & EXIT_MASK));
	else
		mupip_exit(SS_NORMAL);
}
