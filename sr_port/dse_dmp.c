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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "dse.h"
#include "error.h"
#include "util.h"

GBLDEF enum dse_fmt	dse_dmp_format = CLOSED_FMT;

GBLREF boolean_t	patch_is_fdmp;
GBLREF int		patch_fdmp_recs;

#define MESS_OFF SIZEOF("; ") - 1

CONDITION_HANDLER(dse_dmp_handler)
{
	START_CH;
	PRN_ERROR;
	util_out_print("!/DSE is not able to complete the dump to file due to the above reason.!/", TRUE);
	UNWIND(NULL,NULL);
}

static char	*format_label[] = {"; BAD", "; GLO", "; ZWR"}; /* CLOSE_FMT == 0, GLO_FMT == 1 and ZWR_FMT == 2 */

void	dse_dmp(void)
{
	boolean_t	dmp_res, glo_present, zwr_present;

	patch_fdmp_recs = 0;
	glo_present = (CLI_PRESENT == cli_present("GLO"));
	zwr_present = (CLI_PRESENT == cli_present("ZWR"));
	if (glo_present || zwr_present)
	{
		if (CLOSED_FMT == dse_dmp_format)
		{
			util_out_print("Error:  must open an output file before dump.", TRUE);
			return;
		}
		if (gtm_utf8_mode && (GLO_FMT == glo_present))
		{
			util_out_print("Error:  GLO format is not supported in UTF-8 mode. Use ZWR format.", TRUE);
			return;
		}
		if (OPEN_FMT == dse_dmp_format)
		{
			dse_dmp_format = (glo_present ? GLO_FMT : ZWR_FMT);
			if (!gtm_utf8_mode)
				dse_fdmp_output(LIT_AND_LEN("; DSE EXTRACT"));
			else
				dse_fdmp_output(LIT_AND_LEN("; DSE EXTRACT UTF-8"));
			dse_fdmp_output(STR_AND_LEN(format_label[dse_dmp_format]));
		} else if ((glo_present ? GLO_FMT : ZWR_FMT) != dse_dmp_format)
		{
			util_out_print("Error:  current output file already contains !AD records.", TRUE,
					LEN_AND_STR(&format_label[dse_dmp_format][MESS_OFF]));
			return;
		}
		patch_is_fdmp = TRUE;
		ESTABLISH(dse_dmp_handler);
	} else
		patch_is_fdmp = FALSE;
	if (CLI_PRESENT == cli_present("RECORD") || CLI_PRESENT == cli_present("OFFSET"))
		dmp_res = dse_r_dmp();
	else
		dmp_res = dse_b_dmp();
	if (patch_is_fdmp)
	{
		REVERT;
		if (dmp_res)
			util_out_print("!UL !AD records written.!/", TRUE, patch_fdmp_recs,
					LEN_AND_STR(&format_label[dse_dmp_format][MESS_OFF]));
	}
	return;
}
