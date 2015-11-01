/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <signal.h>
#include <errno.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "cli.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_stop.h"
#include "mu_signal_process.h"

void mu_signal_process(char *command, int signal)
{
	short	slen;
	int	len, process_id, save_errno;
	char	buff[256];
	error_def(ERR_MUPCLIERR);

	slen = sizeof(buff);
	if (!cli_get_str("ID", buff, &slen))
		mupip_exit(ERR_MUPCLIERR);
	len = slen;
	process_id = asc2i((uchar_ptr_t)buff, len);
	if (process_id < 0)
	{
		util_out_print("Error converting !AD to a number", FLUSH, len, buff);
		mupip_exit(ERR_MUPCLIERR);
	} else
	{
		if (-1 == kill(process_id, signal))
		{
			save_errno = errno;
			util_out_print("Error issuing !AD to process !UL: !AZ", FLUSH,
				       LEN_AND_STR(command), process_id, STRERROR(errno));
		} else
			util_out_print("!AD issued to process !UL", FLUSH, LEN_AND_STR(command), process_id);
	}
	return;
}
