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
#include "gtm_ctype.h"

#include "stp_parms.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "cli.h"
#include "iosp.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_create.h"
#include "mu_cre_file.h"

GBLREF gd_addr 		*gd_header;
GBLREF gd_region 	*gv_cur_region;

void mupip_create(void)
{
	bool		found;
	char		buff[MAX_RN_LEN + 1], create_stat, exit_stat;
	unsigned short	reglen;
	int		i;
	gd_region	*reg, *reg_top;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_DBNOCRE);

	exit_stat = EXIT_NRM;
	gvinit();
	if (CLI_PRESENT == cli_present("REGION"))
	{
		reglen = SIZEOF(buff);
	 	if (0 == cli_get_str("REGION", buff, &reglen))
			mupip_exit(ERR_MUPCLIERR);
	 	for (i=0; (MAX_RN_LEN + 1 > i) && (' ' != buff[i]); i++)
	 		buff[i] = TOUPPER(buff[i]); /* ensure uppercase to match gde conventions */
	 	for ( ; MAX_RN_LEN + 1 > i; i++)
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
			util_out_print("Error:  region not found.",TRUE);
			mupip_exit(ERR_MUPCLIERR);
		}
		gv_cur_region = reg;
		create_stat = mu_cre_file();
		exit_stat |= create_stat;
	} else
	{
		for (gv_cur_region = gd_header->regions, reg_top = gv_cur_region + gd_header->n_regions;
			gv_cur_region < reg_top; gv_cur_region++)
		{
			create_stat = mu_cre_file();
			exit_stat |= create_stat;
	       	}
		gv_cur_region = NULL;
	}
	if (exit_stat & EXIT_MASK)
		mupip_exit(ERR_DBNOCRE & ~EXIT_MASK | (exit_stat & EXIT_MASK));
	else
		mupip_exit(SS_NORMAL);
}
