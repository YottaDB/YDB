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

#include "gtm_stdlib.h"
#include <errno.h>
#include "gtm_netdb.h"
#include <sys/socket.h>
#if !defined(__MVS__) && !defined(VMS)
#include <sys/socketvar.h>
#endif
#include "gtm_inet.h"
#include <netinet/in.h>
#include "gtm_string.h"
#include <ctype.h>
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "repl_msg.h"
#include "jnl.h"
#include "gtmsource.h"
#include "cli.h"
#include "gtm_stdio.h"
#include "util.h"

#define MAX_SECONDARY_LEN 	(MAX_HOST_NAME_LEN + 11) /* +11 for ':' and
							  * port number */

#define DEFAULT_JNLPOOL_SIZE		(64 * 1024 * 1024)

#define DEFAULT_SHUTDOWN_TIMEOUT	30

#define GTMSOURCE_CONN_PARMS_LEN ((10 + 1) * GTMSOURCE_CONN_PARMS_COUNT - 1)

GBLREF	gtmsource_options_t	gtmsource_options;
GBLREF	boolean_t		update_disable;

int gtmsource_get_opt(void)
{
	boolean_t	secondary, dotted_notation;
	int		tries, index = 0;
	unsigned short	secondary_len;
	char		secondary_sys[MAX_SECONDARY_LEN], *c;
	struct hostent	*sec_hostentry;

	boolean_t	passive;

	boolean_t	log;
	unsigned short	log_file_len;

	boolean_t	buffsize_status;

	boolean_t	filter;
	unsigned short	filter_cmd_len;

	int		timeout_status;

	unsigned short	statslog_val_len;
	char		statslog_val[4]; /* "ON" or "OFF" */

	unsigned short	update_val_len;
	char		update_val[sizeof("DISABLE")]; /* "ENABLE" or "DISABLE" */

	unsigned short	connect_parms_str_len;
	char		*connect_parms_str, tmp_connect_parms_str[GTMSOURCE_CONN_PARMS_LEN + 1];
	char		*connect_parm_token_str, *connect_parm;
	int		connect_parms_index;
	boolean_t	connect_parms_badval;
	int		num_actions;

	memset((char *)&gtmsource_options, 0, sizeof(gtmsource_options));

	num_actions  = 0;
	num_actions += ((gtmsource_options.start = (cli_present("START") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.shut_down = (cli_present("SHUTDOWN") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.activate = (cli_present("ACTIVATE") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.deactivate = (cli_present("DEACTIVATE") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.checkhealth = (cli_present("CHECKHEALTH") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.statslog = (cli_present("STATSLOG") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.showbacklog = (cli_present("SHOWBACKLOG") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.changelog = (cli_present("CHANGELOG") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.stopsourcefilter = (cli_present("STOPSOURCEFILTER") == CLI_PRESENT)) ? 1 : 0);
	num_actions += ((gtmsource_options.update = (cli_present("UPDATE") == CLI_PRESENT)) ? 1 : 0);

	if (1 < num_actions && !gtmsource_options.update)
	{
		util_out_print(
			"Invalid mixing of action qualifiers. Only one of START, "
			"SHUTDOWN, ACTIVATE, DEACTIVATE, CHECKHEALTH, STATSLOG, SHOWBACKLOG, CHANGELOG, STOPSOURCEFILTER "
			"should be specified", TRUE);
		return(-1);
	} else if (0 == num_actions)
	{
		util_out_print(
			"No action qualifier specified. One of START, SHUTDOWN, ACTIVATE, DEACTIVATE, CHECKHEALTH, STATSLOG, "
			"SHOWBACKLOG, CHANGELOG, STOPSOURCEFILTER, UPDATE should be specified", TRUE);
		return(-1);
	}

	if (gtmsource_options.start || gtmsource_options.activate)
	{
		if (secondary = (CLI_PRESENT == cli_present("SECONDARY")))
		{
			secondary_len = MAX_SECONDARY_LEN;
			if (!cli_get_str("SECONDARY", secondary_sys, &secondary_len))
			{
				util_out_print("Error parsing SECONDARY qualifier", TRUE);
				return(-1);
			}
			/* Parse secondary_sys into secondary_host
			 * and secondary_port */
			c = secondary_sys;
			dotted_notation = TRUE;
			while(*c && *c != ':')
			{
				if ('.' != *c && !ISDIGIT(*c))
					dotted_notation = FALSE;
				gtmsource_options.secondary_host[index++] = *c++;
			}
			gtmsource_options.secondary_host[index] = '\0';
			if (':' != *c)
			{
				util_out_print("Secondary port number should be specified", TRUE);
				return(-1);
			}
			errno = 0;
			if (((0 == (gtmsource_options.secondary_port = ATOI(++c))) && (0 != errno))
				|| (0 >= gtmsource_options.secondary_port))
			{
				util_out_print("Error parsing secondary port number !AD", TRUE, LEN_AND_STR(c));
				return(-1);
			}
			/* Validate the specified secondary host name */
			if (dotted_notation)
			{
				if ((in_addr_t)-1 ==
					(gtmsource_options.sec_inet_addr = INET_ADDR(gtmsource_options.secondary_host)))
				{
					util_out_print("Invalid IP address !AD", TRUE,
						LEN_AND_STR(gtmsource_options.secondary_host));
					return(-1);
				}
			} else
			{
				for (tries = 0;
		     	     	     tries < MAX_GETHOST_TRIES &&
		     	     	     !(sec_hostentry = GETHOSTBYNAME(gtmsource_options.secondary_host)) &&
		     	     	     h_errno == TRY_AGAIN;
		     	     	     tries++);
				if (NULL == sec_hostentry)
				{
					util_out_print("Could not find IP address for !AD", TRUE,
						LEN_AND_STR(gtmsource_options.secondary_host));
					return(-1);
				}
				gtmsource_options.sec_inet_addr = ((struct in_addr *)sec_hostentry->h_addr_list[0])->s_addr;
			}
		} else if (gtmsource_options.activate) /* SECONDARY not specified */
		{
			util_out_print("SECONDARY qualifier should be specified with ACTIVATE", TRUE);
			return(-1);
		}
	}

	if (gtmsource_options.start)
	{
		if ((passive = (CLI_PRESENT == cli_present("PASSIVE"))) && secondary)
		{
			util_out_print("Only one of SECONDARY and PASSIVE should be specified, but not both", TRUE);
			return(-1);
		} else if (!passive && !secondary)
		{
			util_out_print("One of SECONDARY or PASSIVE should be specified with START", TRUE);
			return(-1);
		}
		gtmsource_options.mode = ((secondary) ? GTMSOURCE_MODE_ACTIVE : GTMSOURCE_MODE_PASSIVE);
	}

	if (gtmsource_options.start || gtmsource_options.statslog ||
	    gtmsource_options.changelog || gtmsource_options.activate ||
	    gtmsource_options.deactivate)
	{
		log = (cli_present("LOG") == CLI_PRESENT);
		if ((gtmsource_options.start || gtmsource_options.changelog) && !log)
		{
			util_out_print("LOG should be specified with START, or CHANGELOG", TRUE);
			return(-1);
		}
		if (log)
		{
			log_file_len = MAX_FN_LEN + 1;
			if (!cli_get_str("LOG", gtmsource_options.log_file, &log_file_len))
			{
				util_out_print("Error parsing LOG qualifier", TRUE);
				return(-1);
			}
		} else
			gtmsource_options.log_file[0] = '\0';
	}

	if (gtmsource_options.start)
	{
		if (buffsize_status = (CLI_PRESENT == cli_present("BUFFSIZE")))
		{
			if (!cli_get_num("BUFFSIZE", &gtmsource_options.buffsize))
			{
				util_out_print("Error parsing BUFFSIZE qualifier", TRUE);
				return(-1);
			}
			if (MIN_JNLPOOL_SIZE > gtmsource_options.buffsize)
				gtmsource_options.buffsize = MIN_JNLPOOL_SIZE;
		} else
			gtmsource_options.buffsize = DEFAULT_JNLPOOL_SIZE;
		/* Round up buffsize to the nearest (~JNL_WRT_END_MASK + 1)
		 * multiple */
		gtmsource_options.buffsize = ((gtmsource_options.buffsize + ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK);
	}

	if (gtmsource_options.start || gtmsource_options.activate)
	{
		if (CLI_PRESENT == cli_present("CONNECTPARAMS"))
		{
			connect_parms_str_len = GTMSOURCE_CONN_PARMS_LEN + 1;
			if (!cli_get_str("CONNECTPARAMS", tmp_connect_parms_str, &connect_parms_str_len))
			{
				util_out_print("Error parsing CONNECTPARAMS qualifier", TRUE);
				return(-1);
			}
#ifdef VMS
			/* strip the quotes around the string. (DCL doesn't do it) */
			assert('"' == tmp_connect_parms_str[0]);
			assert('"' == tmp_connect_parms_str[connect_parms_str_len - 1]);
			connect_parms_str = &tmp_connect_parms_str[1];
			tmp_connect_parms_str[connect_parms_str_len - 1] = '\0';
#else
			connect_parms_str = &tmp_connect_parms_str[0];
#endif
			for (connect_parms_index =
					GTMSOURCE_CONN_HARD_TRIES_COUNT,
			     connect_parms_badval = FALSE,
			     connect_parm_token_str = connect_parms_str;
			     !connect_parms_badval &&
			     connect_parms_index < GTMSOURCE_CONN_PARMS_COUNT &&
			     (connect_parm = strtok(connect_parm_token_str,
						    GTMSOURCE_CONN_PARMS_DELIM))
			     		   != NULL;
			     connect_parms_index++,
			     connect_parm_token_str = NULL)

			{
				errno = 0;
				if ((0 == (gtmsource_options.connect_parms[connect_parms_index] = ATOI(connect_parm))
						&& 0 != errno) || 0 >= gtmsource_options.connect_parms[connect_parms_index])
					connect_parms_badval = TRUE;
			}
			if (connect_parms_badval)
			{
				util_out_print("Error parsing or invalid value parameter in CONNECTPARAMS", TRUE);
				return(-1);
			}
			if (GTMSOURCE_CONN_PARMS_COUNT != connect_parms_index)
			{
				util_out_print(
					"All CONNECTPARAMS - HARD TRIES, HARD TRIES PERIOD, "
					"SOFT TRIES PERIOD, "
					"ALERT TIME, HEARTBEAT INTERVAL, "
					"MAX HEARBEAT WAIT should be specified", TRUE);
				return(-1);
			}
		} else
		{
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT] = REPL_CONN_HARD_TRIES_COUNT;
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD] = REPL_CONN_HARD_TRIES_PERIOD;
			gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD] = REPL_CONN_SOFT_TRIES_PERIOD;
			gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD] = REPL_CONN_ALERT_ALERT_PERIOD;
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] = REPL_CONN_HEARTBEAT_PERIOD;
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] = REPL_CONN_HEARTBEAT_MAX_WAIT;
		}
		if (gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]<
				gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD])
			gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD] =
				gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD];
		if (gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] <
				gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD])
			gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] =
				gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD];
	}

	if (gtmsource_options.start)
	{
		if (filter = (CLI_PRESENT == cli_present("FILTER")))
		{
			filter_cmd_len = MAX_FILTER_CMD_LEN;
	    		if (!cli_get_str("FILTER", gtmsource_options.filter_cmd, &filter_cmd_len))
			{
				util_out_print("Error parsing FILTER qualifier", TRUE);
				return(-1);
			}
		} else
			gtmsource_options.filter_cmd[0] = '\0';
	}

	if (gtmsource_options.shut_down)
	{
		if ((timeout_status = cli_present("TIMEOUT")) == CLI_PRESENT)
		{
			if (!cli_get_num("TIMEOUT", &gtmsource_options.shutdown_time))
			{
				util_out_print("Error parsing TIMEOUT qualifier", TRUE);
				return(-1);
			}
			if (DEFAULT_SHUTDOWN_TIMEOUT < gtmsource_options.shutdown_time || 0 > gtmsource_options.shutdown_time)
			{
				gtmsource_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
				util_out_print("shutdown TIMEOUT changed to !UL", TRUE, gtmsource_options.shutdown_time);
			}
		} else if (CLI_NEGATED == timeout_status)
			gtmsource_options.shutdown_time = -1;
		else /* TIMEOUT not specified */
			gtmsource_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
	}

	if (gtmsource_options.statslog)
	{
		statslog_val_len = 4; /* max(strlen("ON"), strlen("OFF")) + 1 */
		if (!cli_get_str("STATSLOG", statslog_val, &statslog_val_len))
		{
			util_out_print("Error parsing STATSLOG qualifier", TRUE);
			return(-1);
		}
#ifdef UNIX
		cli_strupper(statslog_val);
#endif
		if (0 == strcmp(statslog_val, "ON"))
			gtmsource_options.statslog = TRUE;
		else if (0 == strcmp(statslog_val, "OFF"))
			gtmsource_options.statslog = FALSE;
		else
		{
			util_out_print("Invalid value for STATSLOG qualifier, should be either ON or OFF", TRUE);
			return(-1);
		}
	}

	if (gtmsource_options.activate)
		update_disable = FALSE;
	if (gtmsource_options.deactivate || (passive && gtmsource_options.start))
		update_disable = TRUE;

	if (gtmsource_options.update)
	{
		update_val_len = sizeof(update_val);
		if (!cli_get_str("UPDATE", update_val, &update_val_len))
		{
			util_out_print("Error parsing UPDATE qualifier", TRUE);
			return(-1);
		}
#ifdef UNIX
		cli_strupper(update_val);
#endif
		if (strcmp(update_val, "ENABLE") == 0)
		{
			util_out_print("Update are enabled now on Secondary", TRUE);
			update_disable = FALSE;
		}
		else if (strcmp(update_val, "DISABLE") == 0)
		{
			util_out_print("Update are disabled now on Secondary", TRUE);
			update_disable = TRUE;
		}
		else
		{
			util_out_print("Invalid value for UPDATE qualifier, should be either ENABLE or DISABLE", TRUE);
			return(-1);
		}
	}
	return(0);
}
