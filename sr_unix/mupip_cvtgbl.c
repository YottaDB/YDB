/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
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
#include <gtm_fcntl.h>
#include <errno.h>

GBLREF	int		gv_fillfactor;
GBLREF	bool		mupip_error_occurred;
GBLREF	boolean_t	is_replicator;
GBLREF	boolean_t	skip_dbtriggers;
GBLREF	mstr		sys_input;
GBLDEF	int		onerror;

error_def(ERR_MUNOFINISH);
error_def(ERR_MUPCLIERR);
error_def(ERR_LOADEDBG);
error_def(ERR_LOADBGSZ);
error_def(ERR_LOADBGSZ2);
error_def(ERR_LOADEDSZ);
error_def(ERR_LOADEDSZ2);

#define	MAX_ONERROR_VALUE_LEN	11	/* PROCEED, STOP, INTERACTIVE are the choices with INTERACTIVE being the maximum */
#define	MAX_FORMAT_VALUE_LEN	 6	/* ZWR, BINARY, GO, GOQ are the choices with BINARY being the longest */

void mupip_cvtgbl(void)
{
	unsigned short	fn_len, len;
	char		fn[MAX_FN_LEN + 1];
	unsigned char	buff[MAX_ONERROR_VALUE_LEN];
	uint4		begin, end;
	int		i, format;
	uint4	        cli_status;
	gtm_int64_t	begin_i8, end_i8;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(MAX_ONERROR_VALUE_LEN > MAX_FORMAT_VALUE_LEN);	/* so the buff[] definition above is good for FORMAT and ONERROR */
	/* If an online rollback occurs when we are loading up the database with new globals and takes us back to a prior logical
	 * state, then we should not continue with the load. The reason being that the application might rely on certain globals to
	 * be present before loading others and that property could be violated if online rollback takes the database back to a
	 * completely different logical state. Set the variable issue_DBROLLEDBACK_anyways that forces the restart logic to issue
	 * an rts_error the first time it detects an online rollback (that takes the database to a prior logical state).
	 */
	TREF(issue_DBROLLEDBACK_anyways) = TRUE;
	is_replicator = TRUE;
	skip_dbtriggers = TRUE;
	fn_len = SIZEOF(fn);
	if (cli_present("STDIN"))
	{
		/* User wants to load from standard input */
		assert(SIZEOF(fn) > sys_input.len);
		memcpy(fn, sys_input.addr, sys_input.len);
		fn_len = sys_input.len;
		assert(-1 != fcntl(fileno(stdin), F_GETFD));
		/* Check if both file name and -STDIN specified. */
		if (cli_get_str("FILE", fn, &fn_len))
			mupip_exit(ERR_MUPCLIERR);
	} else if (!cli_get_str("FILE", fn, &fn_len))  /* User wants to read from a file. */
		mupip_exit(ERR_MUPCLIERR); /* Neither -STDIN nor file name specified. */
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

	if (cli_present("ONERROR") == CLI_PRESENT)
	{
		len = SIZEOF(buff);
		if (!cli_get_str("ONERROR", (char *)buff, &len))
		{
			assert(FALSE);
			onerror = ONERROR_PROCEED;
		} else
		{
			lower_to_upper(buff, buff, len);
			if (!memcmp(buff, "STOP", len))
				onerror = ONERROR_STOP;
			else if (!memcmp(buff, "PROCEED", len))
				onerror = ONERROR_PROCEED;
			else if (!memcmp(buff, "INTERACTIVE", len))
			{
				if (isatty(0)) /*if stdin is a terminal*/
					onerror = ONERROR_INTERACTIVE;
				else
					onerror = ONERROR_STOP;
			} else
			{
				util_out_print("Illegal ONERROR parameter for load",TRUE);
				mupip_exit(ERR_MUPCLIERR);
			}
		}
	} else
		onerror = ONERROR_PROCEED; /* Default: Proceed on error */

	if (cli_present("FORMAT") == CLI_PRESENT)
	{
		len = SIZEOF(buff);
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
	mupip_exit(mupip_error_occurred ? ERR_MUNOFINISH : SS_NORMAL);
}
