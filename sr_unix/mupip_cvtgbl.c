/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"		/* for exit() */

#include "stp_parms.h"
#include "iosp.h"
#include "cli.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"
#include "mupip_cvtgbl.h"
#include "mu_load_input.h"
#include "load.h"
#include "mu_outofband_setup.h"

#define MAX_FILLFACTOR 100
#define MIN_FILLFACTOR 30

GBLREF int	gv_fillfactor;
GBLREF bool	mupip_error_occurred;
GBLREF bool	is_db_updater;

void mupip_cvtgbl(void)
{
	unsigned short	fn_len, len;
	char		fn[256];
	unsigned char	buff[7];
	int		i, begin, end, format;
	uint4	        cli_status;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_LOADEDBG);

	is_db_updater = TRUE;
	fn_len = 256;
	if (!cli_get_str("FILE", fn, &fn_len))
		mupip_exit(ERR_MUPCLIERR);
	mu_load_init(fn, fn_len);
	if (mupip_error_occurred)
		exit(-1);
	mu_outofband_setup();
	if ((cli_status = cli_present("BEGIN")) == CLI_PRESENT)
	{
	        if (!cli_get_num("BEGIN", &begin ))
			mupip_exit(ERR_MUPCLIERR);
		if ( begin < 1)
			begin = 1;
	}
	else
		begin = 0;
	if ((cli_status = cli_present("END")) == CLI_PRESENT)
	{
	        if (!cli_get_num("END", &end ))
			mupip_exit(ERR_MUPCLIERR);
		if ( end < 1)
			end = 1;
		if (end < begin)
			mupip_exit(ERR_LOADEDBG);
	}
	else
		end = 999999999;
	if ((cli_status = cli_present("FILL_FACTOR")) == CLI_PRESENT)
	{
	        if (!cli_get_num("FILL_FACTOR", &gv_fillfactor))
			gv_fillfactor = MAX_FILLFACTOR;
		if ( gv_fillfactor < MIN_FILLFACTOR)
			gv_fillfactor = MIN_FILLFACTOR;
		else if (gv_fillfactor > MAX_FILLFACTOR)
			gv_fillfactor = MAX_FILLFACTOR;
	}
	else
		gv_fillfactor = MAX_FILLFACTOR;

	if (cli_present("FORMAT") == CLI_PRESENT)
	{
	        len = sizeof("FORMAT");
		if (!cli_get_str("FORMAT", (char *)buff, &len))
			go_load(begin, end);
		else
		{
		        lower_to_upper(buff, buff, len);
			if (!memcmp(buff, "ZWR", len))
				go_load(begin, end);
			else if (!memcmp(buff, "BINARY", len))
				bin_load(begin, end);
			else if (!memcmp(buff, "GO", len))
				go_load(begin, end);
			else if (!memcmp(buff, "GOQ", len))
				goq_load();
			else
			{
			        util_out_print("Illegal format for load",TRUE);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
	}
	else
		go_load(begin, end);

	if (mupip_error_occurred)
	{
	        util_out_print("Error occurred during loading",TRUE);
		exit(-1);
	}
	else
		mupip_exit(SS_NORMAL);
}
