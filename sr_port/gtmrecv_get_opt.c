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

#include <stdlib.h>
#include <errno.h>
#include "gtm_string.h"
#include "gtm_unistd.h"
#include <ctype.h>
#include <arpa/inet.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmrecv.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "cli.h"
#include "gtm_stdio.h"
#include "util.h"

#define DEFAULT_RECVPOOL_SIZE	(64 * 1024 * 1024)

#define DEFAULT_SHUTDOWN_TIMEOUT	30

GBLDEF	gtmrecv_options_t	gtmrecv_options;

int gtmrecv_get_opt(void)
{

	boolean_t	log;
	unsigned short	log_file_len;
	boolean_t	buffsize_status;
	boolean_t	filter;
	unsigned short	filter_cmd_len;
	int		timeout_status;
	boolean_t	listenport_status;
	unsigned short	statslog_val_len;
	char		statslog_val[4]; /* "ON" or "OFF" */
	int		num_actions;


	num_actions = 0;
	num_actions += ((gtmrecv_options.start = (CLI_PRESENT == cli_present("START"))) ? 1 : 0);
	num_actions += ((gtmrecv_options.shut_down = (CLI_PRESENT == cli_present("SHUTDOWN"))) ? 1 : 0);
	num_actions += ((gtmrecv_options.checkhealth = (CLI_PRESENT == cli_present("CHECKHEALTH"))) ? 1 : 0);
	num_actions += ((gtmrecv_options.statslog = (CLI_PRESENT == cli_present("STATSLOG"))) ? 1 : 0);
	num_actions += ((gtmrecv_options.showbacklog = (CLI_PRESENT == cli_present("SHOWBACKLOG"))) ? 1 : 0);
	num_actions += ((gtmrecv_options.changelog = (CLI_PRESENT == cli_present("CHANGELOG"))) ? 1 : 0);

	if (1 < num_actions)
	{
		util_out_print("Invalid mixing of action qualifiers. Only one of START, SHUTDOWN, CHECKHEALTH, STATSLOG, "
				"SHOWBACKLOG, CHANGELOG should be specified", TRUE);
		return (-1);
	} else if (0 == num_actions)
	{
		util_out_print("No action qualifier specified. One of START, SHUTDOWN, CHECKHEALTH, STATSLOG, SHOWBACKLOG, "
				"CHANGELOG should be specified", TRUE);
		return (-1);
	}

	gtmrecv_options.updateonly = (CLI_PRESENT == cli_present("UPDATEONLY"));
	gtmrecv_options.updateresync = (CLI_PRESENT == cli_present("UPDATERESYNC"));

	if (gtmrecv_options.start && !gtmrecv_options.updateonly)
	{
		if (!(listenport_status = (CLI_PRESENT == cli_present("LISTENPORT"))))
		{
			util_out_print("LISTENPORT should be specfied with START", TRUE);
			return (-1);
		} else if (!cli_get_num("LISTENPORT", &gtmrecv_options.listen_port))
		{
			util_out_print("Error parsing LISTENPORT qualifier", TRUE);
			return (-1);
		}

		if (buffsize_status = (CLI_PRESENT == cli_present("BUFFSIZE")))
		{
			if (!cli_get_num("BUFFSIZE", &gtmrecv_options.buffsize))
			{
				util_out_print("Error parsing BUFFSIZE qualifier", TRUE);
				return (-1);
			}
			if (MIN_RECVPOOL_SIZE > gtmrecv_options.buffsize)
				gtmrecv_options.buffsize = MIN_RECVPOOL_SIZE;
		} else
			gtmrecv_options.buffsize = DEFAULT_RECVPOOL_SIZE;
		/* Round up or down buffsize */

		if (filter = (CLI_PRESENT == cli_present("FILTER")))
		{
			filter_cmd_len = MAX_FILTER_CMD_LEN;
	    		if (!cli_get_str("FILTER", gtmrecv_options.filter_cmd, &filter_cmd_len))
			{
				util_out_print("Error parsing FILTER qualifier", TRUE);
				return (-1);
			}
		} else
			gtmrecv_options.filter_cmd[0] = '\0';

		gtmrecv_options.stopsourcefilter = (CLI_PRESENT == cli_present("STOPSOURCEFILTER"));
	}

	if ((gtmrecv_options.start && !gtmrecv_options.updateonly) || gtmrecv_options.statslog || gtmrecv_options.changelog)
	{
		log = (CLI_PRESENT == cli_present("LOG"));
		if ((gtmrecv_options.start && !gtmrecv_options.updateonly || gtmrecv_options.changelog) && !log)
		{
			util_out_print("LOG should be specified with START or CHANGELOG", TRUE);
			return (-1);
		}
		if (log)
		{
			log_file_len = MAX_FN_LEN + 1;
			if (!cli_get_str("LOG", gtmrecv_options.log_file, &log_file_len))
			{
				util_out_print("Error parsing LOG qualifier", TRUE);
				return (-1);
			}
		} else
			gtmrecv_options.log_file[0] = '\0';
	}

	if (!gtmrecv_options.start && gtmrecv_options.updateresync)
	{
		util_out_print("UPDATERESYNC can be used only with START", TRUE);
		gtmrecv_options.updateresync = FALSE;
		return (-1);
	}

	if (gtmrecv_options.shut_down)
	{
		if (CLI_PRESENT == (timeout_status = cli_present("TIMEOUT")))
		{
			if (!cli_get_num("TIMEOUT", &gtmrecv_options.shutdown_time))
			{
				util_out_print("Error parsing TIMEOUT qualifier", TRUE);
				return (-1);
			}
			if (DEFAULT_SHUTDOWN_TIMEOUT < gtmrecv_options.shutdown_time || 0 > gtmrecv_options.shutdown_time)
			{
				gtmrecv_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
				util_out_print("shutdown TIMEOUT changed to !UL", TRUE, gtmrecv_options.shutdown_time);
			}
		} else if (CLI_NEGATED == timeout_status)
			gtmrecv_options.shutdown_time = -1;
		else /* TIMEOUT not specified */
			gtmrecv_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
	}

	if (gtmrecv_options.statslog)
	{
		statslog_val_len = 4; /* max(strlen("ON"), strlen("OFF")) + 1 */
		if (!cli_get_str("STATSLOG", statslog_val, &statslog_val_len))
		{
			util_out_print("Error parsing STATSLOG qualifier", TRUE);
			return (-1);
		}
#ifdef UNIX
		cli_strupper(statslog_val);
#endif
		if (0 == strcmp(statslog_val, "ON"))
			gtmrecv_options.statslog = TRUE;
		else if (0 == strcmp(statslog_val, "OFF"))
			gtmrecv_options.statslog = FALSE;
		else
		{
			util_out_print("Invalid value for STATSLOG qualifier, should be either ON or OFF", TRUE);
			return (-1);
		}
	}

	return (0);
}
