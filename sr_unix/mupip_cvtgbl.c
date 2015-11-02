/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services, Inc	*
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

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "stp_parms.h"
#include "iosp.h"
#include "cli.h"
#include "util.h"
#include "gtm_caseconv.h"
#include "mupip_exit.h"
#include "mupip_cvtgbl.h"
#include "file_input.h"
#include "load.h"
#include "mu_outofband_setup.h"

GBLREF	int		gv_fillfactor;
GBLREF	bool		mupip_error_occurred;
GBLREF	boolean_t	is_replicator;
GBLREF	boolean_t	skip_dbtriggers;

void mupip_cvtgbl(void)
{
	unsigned short	fn_len, len;
	char		fn[256];
	unsigned char	buff[7];
	uint4		begin, end;
	int		i, format;
	uint4	        cli_status;
	gtm_int64_t	begin_i8, end_i8;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_LOADEDBG);
	error_def(ERR_LOADBGSZ);
	error_def(ERR_LOADBGSZ2);
	error_def(ERR_LOADEDSZ);
	error_def(ERR_LOADEDSZ2);

	is_replicator = TRUE;
	skip_dbtriggers = TRUE;
	fn_len = 256;
	if (!cli_get_str("FILE", fn, &fn_len))
		mupip_exit(ERR_MUPCLIERR);
	file_input_init(fn, fn_len);
	if (mupip_error_occurred)
		exit(-1);
	mu_outofband_setup();
	if ((cli_status = cli_present("BEGIN")) == CLI_PRESENT)
	{
	        if (!cli_get_int64("BEGIN", &begin_i8))
			mupip_exit(ERR_MUPCLIERR);
		if (1 > begin_i8)
			mupip_exit(ERR_LOADBGSZ);
		else if (MAXUINT4 < begin_i8)
			mupip_exit(ERR_LOADBGSZ2);
		begin = (uint4) begin_i8;
	} else
	{
		begin = 1;
		begin_i8 = 1;
	}
	if ((cli_status = cli_present("END")) == CLI_PRESENT)
	{
	        if (!cli_get_int64("END", &end_i8))
			mupip_exit(ERR_MUPCLIERR);
		if (1 > end_i8)
			mupip_exit(ERR_LOADEDSZ);
		else if (MAXUINT4 < end_i8)
			mupip_exit(ERR_LOADEDSZ2);
		if (end_i8 < begin_i8)
			mupip_exit(ERR_LOADEDBG);
		end = (uint4) end_i8;
	} else
		end = MAXUINT4;
	if ((cli_status = cli_present("FILL_FACTOR")) == CLI_PRESENT)
	{
		assert(SIZEOF(gv_fillfactor) == SIZEOF(int4));
	        if (!cli_get_int("FILL_FACTOR", (int4 *)&gv_fillfactor))
			gv_fillfactor = MAX_FILLFACTOR;
		if (gv_fillfactor < MIN_FILLFACTOR)
			gv_fillfactor = MIN_FILLFACTOR;
		else if (gv_fillfactor > MAX_FILLFACTOR)
			gv_fillfactor = MAX_FILLFACTOR;
	} else
		gv_fillfactor = MAX_FILLFACTOR;

	if (cli_present("FORMAT") == CLI_PRESENT)
	{
	        len = SIZEOF("FORMAT");
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
	} else
		go_load(begin, end);

	if (mupip_error_occurred)
	{
	        util_out_print("Error occurred during loading",TRUE);
		exit(-1);
	}
	else
		mupip_exit(SS_NORMAL);
}
