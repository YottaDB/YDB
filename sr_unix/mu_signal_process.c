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

#include <signal.h>
#include <errno.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "cli.h"
#include "util.h"
#include "mupip_exit.h"
#include "mupip_stop.h"
#include "mu_signal_process.h"
#include "send_msg.h"

GBLREF uint4	process_id;

void mu_signal_process(char *command, int signal)
{
	unsigned short	slen;
	int		len, toprocess_id, save_errno;
	char		buff[256];
	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUPIPSIG);

	slen = SIZEOF(buff);
	if (!cli_get_str("ID", buff, &slen))
		mupip_exit(ERR_MUPCLIERR);
	len = slen;
	toprocess_id = asc2i((uchar_ptr_t)buff, len);
	if (toprocess_id < 0)
	{
		util_out_print("Error converting !AD to a number", FLUSH, len, buff);
		mupip_exit(ERR_MUPCLIERR);
	} else
	{
		if (-1 == kill(toprocess_id, signal))
		{
			save_errno = errno;
			util_out_print("Error issuing !AD to process !UL: !AZ", FLUSH,
				       LEN_AND_STR(command), toprocess_id, STRERROR(errno));
		} else
		{
			util_out_print("!AD issued to process !UL", FLUSH, LEN_AND_STR(command), toprocess_id);
			if (!MEMCMP_LIT(command, STOP_STR))
			{
				send_msg(VARLSTCNT(9) ERR_MUPIPSIG, 7, LEN_AND_STR(command), signal, process_id, process_id,
					 toprocess_id, toprocess_id);
			}
		}
	}
	return;
}
