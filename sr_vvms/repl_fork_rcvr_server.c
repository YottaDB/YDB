/****************************************************************
 *								*
 *	Copyright 2005, 2009 Fidelity Information Services, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_string.h"
#include <ssdef.h>
#include <descrip.h>

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "repl_sp.h"
#include "gdsfhead.h"
#include "gtmrecv.h"
#include "cli.h"

error_def(ERR_JNLPOOLSETUP);
error_def(ERR_RECVPOOLSETUP);


GBLREF gtmrecv_options_t	gtmrecv_options;

int repl_fork_rcvr_server(uint4 *pid, uint4 *cmd_channel)
{
	int		status, retval=0;
	char		cmd_str[MAX_COMMAND_LINE_LENGTH];
	uint4		flags, channel;
	unsigned short	cmd_len, mbsb[4];
	uint4		buff, server_pid, clus_flags_stat;
	mstr		log_nam, trans_log_nam;
	gds_file_id	file_id;
	unsigned short	log_file_len;
	int		exp_log_file_name_len, len;
	char		*ptr, qwstring[100];

	$DESCRIPTOR(cmd_desc, cmd_str);

	/* Get the cmd line */
	if (((status = lib$get_foreign(&cmd_desc, 0, &cmd_len)) & 1) == 0)
		return status;
	if (0 == cmd_len)
	{ /* Command issued on the MUPIP command line, we have to build the argument string to pass to child */
		MEMCPY_LIT(&cmd_str[cmd_len], RECV_PROMPT_START_QUAL);
		cmd_len += SIZEOF(RECV_PROMPT_START_QUAL) - 1;
		if (CLI_PRESENT == cli_present("BUFFSIZE"))
		{
			MEMCPY_LIT(&cmd_str[cmd_len], BUFF_QUAL);
			cmd_len += SIZEOF(BUFF_QUAL) - 1;
			ptr = i2asc(qwstring, gtmrecv_options.buffsize);
			memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
			cmd_len += ptr - qwstring;
		}
		if (CLI_PRESENT == cli_present("FILTER"))
		{
			MEMCPY_LIT(&cmd_str[cmd_len], FILTER_QUAL);
			cmd_len += SIZEOF(FILTER_QUAL) - 1;
			len = strlen(gtmrecv_options.filter_cmd);
			memcpy(&cmd_str[cmd_len], gtmrecv_options.filter_cmd, len);
			cmd_len += len;
		}
		if (CLI_PRESENT == cli_present("LOG"))
		{
			MEMCPY_LIT(&cmd_str[cmd_len], LOG_QUAL);
			cmd_len += SIZEOF(LOG_QUAL) - 1;
			len = strlen(gtmrecv_options.log_file);
			memcpy(&cmd_str[cmd_len], gtmrecv_options.log_file, len);
			cmd_len += len;
		}
		if (CLI_PRESENT == cli_present("LOG_INTERVAL"))
		{
			MEMCPY_LIT(&cmd_str[cmd_len], LOGINTERVAL_QUAL);
			cmd_len += SIZEOF(LOGINTERVAL_QUAL) - 1;
			cmd_str[cmd_len++] = '"'; /* begin quote */
			ptr = i2asc(qwstring, gtmrecv_options.rcvr_log_interval);
			memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
			cmd_len += ptr - qwstring;
			cmd_str[cmd_len++] = ','; /* delimiter */
			ptr = i2asc(qwstring, gtmrecv_options.upd_log_interval);
			memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
			cmd_len += ptr - qwstring;
			cmd_str[cmd_len++] = '"'; /* end quote */
		}
		if (CLI_PRESENT == cli_present("LISTENPORT"))
		{
			MEMCPY_LIT(&cmd_str[cmd_len], LISTENPORT_QUAL);
			cmd_len += SIZEOF(LISTENPORT_QUAL) - 1;
			ptr = i2asc(qwstring, gtmrecv_options.listen_port);
			memcpy(&cmd_str[cmd_len], qwstring, ptr - qwstring);
			cmd_len += ptr - qwstring;
		}
	}

	/* Append a dummy qualifier */
	MEMCPY_LIT(&cmd_str[cmd_len], DUMMY_START_QUAL);
	cmd_desc.dsc$w_length = cmd_len + SIZEOF(DUMMY_START_QUAL) - 1;
	/* Create detached server and write startup commands to it */
	return repl_create_server(&cmd_desc, "GTMR", "", cmd_channel, pid, ERR_RECVPOOLSETUP);
}
