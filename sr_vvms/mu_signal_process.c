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
#include <descrip.h>
#include <climsgdef.h>
#include <ssdef.h>
#include <jpidef.h>
#include <signal.h>
#include "gtm_string.h"
#include "cli.h"
#include "util.h"
#include "mu_signal_process.h"
#include "send_msg.h"

static	int 	send_signal(int, int);
GBLREF	uint4	process_id;

#define SENDMSG_OUTPUT(mpname, mpid) 								 \
{												 \
	error_def(ERR_MUPIPSIG);								 \
	if (!MEMCMP_LIT(command, STOP_STR))							 \
		send_msg(VARLSTCNT(9) ERR_MUPIPSIG, 7, LEN_AND_STR(command), signal, process_id, \
			process_id, mpid, mpid); 						 \
	util_out_print("!AD issued to process !AD: (PID=!XL)",  FLUSH, LEN_AND_STR(command), 	 \
		LEN_AND_STR(mpname), mpid);							 \
}

static int send_signal(int pid, int signal)
{
	int status;

	if (SIGUSR1 == signal)
	{	/* Currently only type of posix signal used */
		status = kill(pid, signal);
		if (-1 == status)
		{
			perror("Job Interrupt request failed: ");
			status = SS$_BADPARAM;
		} else
			status = SS$_NORMAL;
	} else
	{	/* Default signal but only ERR_FORCEDHALT currently sent */
		status = sys$forcex(&pid, 0, signal);
		if (status != SS$_NORMAL)
			rts_error(VARLSTCNT(1) status);
	}
	return status;
}

void mu_signal_process(char *command, int signal)
{
	boolean_t	pid_present, name_present;
	int4		pid, length, status, item, outv;
	char		prc_nam[20];
	unsigned short	name_len;
	$DESCRIPTOR(d_prc_nam,"");

	memset(prc_nam, 0, SIZEOF(prc_nam));
	pid_present = name_present = FALSE;
	if (cli_present("id") == CLI_PRESENT)
	{
		if(!cli_get_hex("id", &pid))
			return;
		pid_present = TRUE;
	}
	if (cli_present("name") == CLI_PRESENT)
	{
		name_len = 20;
		if (!cli_get_str("name", prc_nam, &name_len))
			return;
		if (prc_nam[name_len-1] == '"')
			name_len--;
		if (prc_nam[0] == '"')
		{
			d_prc_nam.dsc$a_pointer = &prc_nam[1];
			name_len--;
		} else
			d_prc_nam.dsc$a_pointer = &prc_nam;
		d_prc_nam.dsc$w_length = name_len;
		name_present = TRUE;
	}
	if (!name_present)
	{
		if (SS$_NORMAL == send_signal(pid, signal))
			SENDMSG_OUTPUT("", pid);
		return;
	}
	item = JPI$_PID;
	status = lib$getjpi(&item, 0, &d_prc_nam, &outv, 0, 0);
	if (SS$_NORMAL != status)
	{
		rts_error(VARLSTCNT(1) status);
		return;
	}
	if (!pid_present)
	{
		if (SS$_NORMAL == send_signal(outv, signal))
			SENDMSG_OUTPUT(&prc_nam, outv);
		return;
	}
	if (outv != pid)
	{
		util_out_print("ID !XL and NAME !AD are not the same process", FLUSH, pid, LEN_AND_STR(&prc_nam));
		return;
	}
	if (SS$_NORMAL == send_signal(pid, signal))
		SENDMSG_OUTPUT(&prc_nam, pid);
	return;
}
