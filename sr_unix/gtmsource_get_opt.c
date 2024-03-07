/****************************************************************
 *								*
 * Copyright (c) 2006-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"
#include "gtm_netdb.h"
#include "gtm_socket.h"
#include "gtm_inet.h"
#include "gtm_string.h"
#include "gtm_ctype.h"
#if !defined(__MVS__) && !defined(__CYGWIN__) && (!defined(__GNUC__) && defined(__hpux))
#include <sys/socketvar.h>
#endif
#include <errno.h>

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
#include "repl_log.h"
#include "gtmmsg.h"		/* for "gtm_putmsg" prototype */
#include "ydb_trans_log_name.h"
#include "iosp.h"		/* for SS_NORMAL */
#include "gtm_zlib.h"
#ifdef GTM_TLS
#include "gtm_repl.h"
#endif

#define MAX_SECONDARY_LEN 		(MAX_HOST_NAME_LEN + 11) /* +11 for ':' and port number */
#define DEFAULT_SHUTDOWN_TIMEOUT	120
#define MAX_SHUTDOWN_TIMEOUT		3600 /* 1 hour */
#define GTMSOURCE_CONN_PARMS_LEN 	((10 + 1) * GTMSOURCE_CONN_PARMS_COUNT - 1)

GBLREF	gtmsource_options_t	gtmsource_options;
#ifdef GTM_TLS
GBLREF	repl_tls_info_t		repl_tls;
#endif
GBLREF	volatile boolean_t	timer_in_handler;
GBLREF  gd_addr                 *gd_header;

LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

error_def(ERR_LOGTOOLONG);
error_def(ERR_REPLINSTSECLEN);
error_def(ERR_REPLINSTSECUNDF);
error_def(ERR_INVSHUTDOWN);

int gtmsource_get_opt(void)
{
	boolean_t	connect_parms_badval, connect_parm_digit, log, log_interval_specified, plaintext_fallback, secondary;
	boolean_t	is_ydb_env_match;
	char		*c, *connect_parm, *connect_parms_str, *connect_parm_token_str;
	char		tmp_connect_parms_str[GTMSOURCE_CONN_PARMS_LEN + 1];
	char		freeze_comment[SIZEOF(gtmsource_options.freeze_comment)];
	char		freeze_val[SIZEOF("OFF")]; /* "ON" or "OFF", including null terminator */
	char		inst_name[MAX_FN_LEN + 1], *ip_end, *strtokptr;
	char		secondary_sys[MAX_SECONDARY_LEN];
	char		statslog_val[SIZEOF("OFF")]; /* "ON" or "OFF" */
	char		update_val[SIZEOF("DISABLE")]; /* "ENABLE" or "DISABLE" */
	gtm_int64_t	buffsize;
	unsigned short	connect_parms_index;
	int 		index = 0, port_len, renegotiate_interval, status;
	int		timeout_status;
	mstr		trans_name;
	struct hostent	*sec_hostentry;
	unsigned short	connect_parms_str_len, filter_cmd_len, freeze_comment_len, freeze_val_len, inst_name_len, log_file_len;
	unsigned short	secondary_len, statslog_val_len, tlsid_len, update_val_len;
	char		*envname;

	memset((char *)&gtmsource_options, 0, SIZEOF(gtmsource_options));
	gtmsource_options.start = (CLI_PRESENT == cli_present("START"));
	gtmsource_options.shut_down = (CLI_PRESENT == cli_present("SHUTDOWN"));
	gtmsource_options.zerobacklog = (CLI_PRESENT == cli_present("ZEROBACKLOG"));
	gtmsource_options.activate = (CLI_PRESENT == cli_present("ACTIVATE"));
	gtmsource_options.deactivate = (CLI_PRESENT == cli_present("DEACTIVATE"));
	gtmsource_options.checkhealth = (CLI_PRESENT == cli_present("CHECKHEALTH"));
	gtmsource_options.statslog = (CLI_PRESENT == cli_present("STATSLOG"));
	gtmsource_options.showbacklog = (CLI_PRESENT == cli_present("SHOWBACKLOG"));
	gtmsource_options.changelog = (CLI_PRESENT == cli_present("CHANGELOG"));
	gtmsource_options.stopsourcefilter = (CLI_PRESENT == cli_present("STOPSOURCEFILTER"));
	gtmsource_options.needrestart = (CLI_PRESENT == cli_present("NEEDRESTART"));
	gtmsource_options.losttncomplete = (CLI_PRESENT == cli_present("LOSTTNCOMPLETE"));
	gtmsource_options.jnlpool = (CLI_PRESENT == cli_present("JNLPOOL"));
	secondary = (CLI_PRESENT == cli_present("SECONDARY"));
	gtmsource_options.rootprimary = ROOTPRIMARY_UNSPECIFIED; /* to indicate unspecified state */
	if ((CLI_PRESENT == cli_present("ROOTPRIMARY")) || (CLI_PRESENT == cli_present("UPDOK")))
		gtmsource_options.rootprimary = ROOTPRIMARY_SPECIFIED;
	else if ((CLI_PRESENT == cli_present("PROPAGATEPRIMARY")) || (CLI_PRESENT == cli_present("UPDNOTOK")))
		gtmsource_options.rootprimary = PROPAGATEPRIMARY_SPECIFIED;
	else
	{	/* Neither ROOTPRIMARY (or UPDOK) nor PROPAGATEPRIMARY (or UPDNOTOK) specified. Assume default values.
		 * Assume ROOTPRIMARY for -START -SECONDARY (active source server start) and -ACTIVATE commands.
		 * Assume PROPAGATEPRIMARY for -START -PASSIVE (passive source server start) and -DEACTIVATE commands.
		 */
		if ((gtmsource_options.start && secondary) || gtmsource_options.activate)
			gtmsource_options.rootprimary = ROOTPRIMARY_SPECIFIED;
		if ((gtmsource_options.start && !secondary) || gtmsource_options.deactivate)
			gtmsource_options.rootprimary = PROPAGATEPRIMARY_SPECIFIED;
	}
	gtmsource_options.instsecondary = (CLI_PRESENT == cli_present("INSTSECONDARY"));
	if (gtmsource_options.instsecondary)
	{	/* -INSTSECONDARY is specified in the command line. */
		inst_name_len = SIZEOF(inst_name);;
		if (!cli_get_str("INSTSECONDARY", &inst_name[0], &inst_name_len))
		{
			util_out_print("Error parsing INSTSECONDARY qualifier", TRUE);
			return(-1);
		}
	} else
	{	/* Check if environment variable "ydb_repl_instsecondary" is defined.
		 * Do that only if any of the following qualifiers is present as these are the only ones that honour it.
		 * 	Mandatory : START, ACTIVATE, DEACTIVATE, STOPSOURCEFILTER, CHANGELOG, STATSLOG, NEEDRESTART,
		 *	Optional  : CHECKHEALTH, SHOWBACKLOG or SHUTDOWN
		 */
		inst_name_len = 0; /* 4SCA: Even though this is guarded by gtmsource_options.instsecondary */
		if (gtmsource_options.start || gtmsource_options.activate || gtmsource_options.deactivate
			|| gtmsource_options.stopsourcefilter || gtmsource_options.changelog
			|| gtmsource_options.statslog || gtmsource_options.needrestart
			|| gtmsource_options.checkhealth || gtmsource_options.showbacklog || gtmsource_options.shut_down)
		{
			trans_name.addr = &inst_name[0];
			if (SS_NORMAL == (status = ydb_trans_log_name(YDBENVINDX_REPL_INSTSECONDARY, &trans_name, inst_name, SIZEOF(inst_name),
									IGNORE_ERRORS_TRUE, &is_ydb_env_match)))
			{
				gtmsource_options.instsecondary = TRUE;
				inst_name_len = trans_name.len;
			} else if (!gtmsource_options.checkhealth && !gtmsource_options.showbacklog && !gtmsource_options.shut_down)
			{
				if (SS_LOG2LONG == status)
				{
					envname = (char *)(is_ydb_env_match ? ydbenvname[YDBENVINDX_REPL_INSTSECONDARY]
									: gtmenvname[YDBENVINDX_REPL_INSTSECONDARY]);
					gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_LOGTOOLONG, 3,
						LEN_AND_STR(envname), SIZEOF(inst_name) - 1);
				}
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_REPLINSTSECUNDF);
				return (-1);
			}
		}
	}
	if (gtmsource_options.instsecondary)
	{	/* Secondary instance name specified either through -INSTSECONDARY or "ydb_repl_instsecondary" */
		inst_name[inst_name_len] = '\0';
		if ((MAX_INSTNAME_LEN <= inst_name_len) || (0 == inst_name_len))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_REPLINSTSECLEN, 2, inst_name_len, inst_name);
			return (-1);
		}
		assert((inst_name_len + 1) <= MAX_INSTNAME_LEN);
		memcpy(gtmsource_options.secondary_instname, inst_name, inst_name_len + 1);	/* copy terminating '\0' as well */
	}
	if (gtmsource_options.start || gtmsource_options.activate)
	{
		if (secondary)
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
			if ('[' == *c)
			{
				ip_end = strchr(++c, ']');
				if (NULL == ip_end || 0 == (index = ip_end - c))
				{
					util_out_print("Invalid IP address !AD", TRUE,
						LEN_AND_STR(secondary_sys));
					return(-1);
				}
				memcpy(gtmsource_options.secondary_host, c, index);
				gtmsource_options.secondary_host[index] = '\0';
				c = ip_end + 1;
			} else
			{
				while(*c && (':' != *c))
					gtmsource_options.secondary_host[index++] = *c++;
				gtmsource_options.secondary_host[index] = '\0';
			}
			if (':' != *c)
			{
				util_out_print("Secondary port number should be specified", TRUE);
				return(-1);
			}
			++c;
			errno = 0;
			if (((0 == (gtmsource_options.secondary_port = ATOI(c))) && (0 != errno))
				|| (0 >= gtmsource_options.secondary_port))
			{
				util_out_print("Error parsing secondary port number !AD", TRUE, LEN_AND_STR(c));
				return(-1);
			}
		}
		gtmsource_options.trigupdate = (CLI_PRESENT == cli_present("TRIGUPDATE"));
		/* Initialize default values for CONNECT_PARMS */
		gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_COUNT] = REPL_CONN_HARD_TRIES_COUNT;
		gtmsource_options.connect_parms[GTMSOURCE_CONN_HARD_TRIES_PERIOD] = REPL_CONN_HARD_TRIES_PERIOD;
		gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD] = REPL_CONN_SOFT_TRIES_PERIOD;
		gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD] = REPL_CONN_ALERT_ALERT_PERIOD;
		gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] = REPL_CONN_HEARTBEAT_PERIOD;
		gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] = REPL_CONN_HEARTBEAT_MAX_WAIT;
		if (CLI_PRESENT == cli_present("CONNECTPARAMS"))
		{	/* Override default values noted above */
			connect_parms_str_len = GTMSOURCE_CONN_PARMS_LEN + 1;
			if (!cli_get_str("CONNECTPARAMS", tmp_connect_parms_str, &connect_parms_str_len))
			{
				util_out_print("Error parsing CONNECTPARAMS qualifier", TRUE);
				return -1;
			}
			connect_parms_str = &tmp_connect_parms_str[0];
			for (connect_parms_index = GTMSOURCE_CONN_HARD_TRIES_COUNT,
				connect_parms_badval = FALSE,
				connect_parm_token_str = connect_parms_str;
			     !connect_parms_badval
				&& (GTMSOURCE_CONN_PARMS_COUNT >= connect_parms_index)
				&& (NULL != (connect_parm =	/* Warning: Assignment inside boolean expression */
						STRTOK_R(connect_parm_token_str, GTMSOURCE_CONN_PARMS_DELIM, &strtokptr)));
			     connect_parms_index++, connect_parm_token_str = NULL)
			{
				size_t	counter;

				if (GTMSOURCE_CONN_PARMS_COUNT > connect_parms_index)
				{
					errno = 0;
					if ((0 == (gtmsource_options.connect_parms[connect_parms_index] = ATOI(connect_parm))
							&& (0 != errno)) /* WARNING assignment above */
							|| (0 >= gtmsource_options.connect_parms[connect_parms_index]))
						connect_parms_badval = TRUE;
				}
				connect_parm_digit = TRUE;
				for (counter = 0; counter < strlen(connect_parm); counter++)
					if (!ISDIGIT_ASCII(connect_parm[counter]))
					{
						connect_parm_digit = FALSE;
						connect_parms_badval = TRUE;
						break;
					}
				if (connect_parms_badval)
				{
					switch(connect_parms_index)
					{	/*Addition validation and error reporting for each param in -CONNECTPARAM */
						 case GTMSOURCE_CONN_HARD_TRIES_COUNT:
							if ((connect_parm_digit) && (0 == ATOI(connect_parm)))
							{
								connect_parms_badval = FALSE;
								break;
							} else
							{
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
									2, LEN_AND_LIT("number of hard connection attempts"),
									ERR_TEXT, 2,
									LEN_AND_LIT("Specify either the number of hard connection"
									" attempts or 0 to disable hard connection attempts")) ;
								return -1;
							}
						case GTMSOURCE_CONN_HARD_TRIES_PERIOD:
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
								2, LEN_AND_LIT("wait time for hard connection attempts"),
								ERR_TEXT, 2,
								LEN_AND_LIT("Specify the wait time in milliseconds"));
							return -1;
						case GTMSOURCE_CONN_SOFT_TRIES_PERIOD:
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
								2, LEN_AND_LIT("wait time for soft connection attempts"),
								ERR_TEXT, 2,
								LEN_AND_LIT("Specify the wait time in seconds"));
							return -1;
						case GTMSOURCE_CONN_ALERT_PERIOD:
							if ((connect_parm_digit) && (0 == ATOI(connect_parm)))
							{
								connect_parms_badval = FALSE;
								break;
							}
							else
							{
								gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
									2, LEN_AND_LIT("alert period for REPLALERT messages"),
									ERR_TEXT, 2,
									LEN_AND_LIT("Specify the approximate alert period (in "
									"seconds) for REPLALERT messages or 0 to disable "
									"REPLALERT messages"));
								return -1;
							}
						 case GTMSOURCE_CONN_HEARTBEAT_PERIOD:
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
								2, LEN_AND_LIT("heartbeat interval"),
								ERR_TEXT, 2,
								LEN_AND_LIT("Specify the interval (in seconds) between "
								"heartbeats to the Receiver Server"));
							return -1;
						case GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT:
							gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
								2, LEN_AND_LIT("maximum heartbeat wait period"),
								ERR_TEXT, 2,
								LEN_AND_LIT("Specify the maximum period (in seconds) for "
								"which the Source Server should wait to receive a heartbeat "
								"response from the Receiver Server"));
							return -1;
					}
				}
			}
			if (GTMSOURCE_CONN_PARMS_COUNT < connect_parms_index)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (1) ERR_BADPARAMCOUNT);
				return -1;
			}
			if ((0 != gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]) &&
				(gtmsource_options.connect_parms[GTMSOURCE_CONN_ALERT_PERIOD]<
					gtmsource_options.connect_parms[GTMSOURCE_CONN_SOFT_TRIES_PERIOD]))
			{

				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
					2, LEN_AND_LIT("alert period for REPLALERT messages"),
					ERR_TEXT, 2,
					LEN_AND_LIT("Alert period cannot be less than soft connection attempts period"));
				return -1;
			}
			if (gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_MAX_WAIT] <
				gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD])
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT (8) ERR_BADCONNECTPARAM,
					2, LEN_AND_LIT("maximum heartbeat wait period"),
					ERR_TEXT, 2,
					LEN_AND_LIT("Maximum heartbeat wait period cannot be less than heartbeat interval"));
				return -1;
			}
		}
	}	/* End of -START || -ACTIVATE */
	if (gtmsource_options.start || gtmsource_options.statslog || gtmsource_options.changelog || gtmsource_options.activate)
	{
		log = (cli_present("LOG") == CLI_PRESENT);
		log_interval_specified = (CLI_PRESENT == cli_present("LOG_INTERVAL"));
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
		gtmsource_options.src_log_interval = 0;
		if (log_interval_specified)
		{
			if (!cli_get_num("LOG_INTERVAL", (int4 *)&gtmsource_options.src_log_interval))
			{
				util_out_print("Error parsing LOG_INTERVAL qualifier", TRUE);
				return (-1);
			}
		}
		if (gtmsource_options.start && 0 == gtmsource_options.src_log_interval)
			gtmsource_options.src_log_interval = LOGTRNUM_INTERVAL;
		/* For changelog/activate, interval == 0 implies don't change log interval already established */
		/* We ignore interval specification for statslog, Vinaya 2005/02/07 */
	}
	if (gtmsource_options.start)
	{
		assert(secondary || CLI_PRESENT == cli_present("PASSIVE"));
		gtmsource_options.mode = ((secondary) ? GTMSOURCE_MODE_ACTIVE : GTMSOURCE_MODE_PASSIVE);
		if (CLI_PRESENT == cli_present("BUFFSIZE"))
		{	/* use a big conversion so we have a signed number for comparison */
			if (!cli_get_int64("BUFFSIZE", &buffsize))
			{
				util_out_print("Error parsing BUFFSIZE qualifier", TRUE);
				return(-1);
			}
			if (MIN_JNLPOOL_SIZE > buffsize)
				gtmsource_options.buffsize = MIN_JNLPOOL_SIZE;
			else if ((gtm_int64_t)MAX_JNLPOOL_SIZE < buffsize)
				gtmsource_options.buffsize = (gtm_uint64_t)MAX_JNLPOOL_SIZE;
			else
				gtmsource_options.buffsize = (gtm_uint64_t)buffsize;
		} else
			gtmsource_options.buffsize = DEFAULT_JNLPOOL_SIZE;
		/* Round up buffsize to the nearest (~JNL_WRT_END_MASK + 1) multiple */
		gtmsource_options.buffsize = ((gtmsource_options.buffsize + ~JNL_WRT_END_MASK) & JNL_WRT_END_MASK);
		if (CLI_PRESENT == cli_present("FILTER"))
		{
			filter_cmd_len = MAX_FILTER_CMD_LEN;
	    		if (!cli_get_str("FILTER", gtmsource_options.filter_cmd, &filter_cmd_len))
			{
				util_out_print("Error parsing FILTER qualifier", TRUE);
				return(-1);
			}
		} else
			gtmsource_options.filter_cmd[0] = '\0';
		/* Check if compression level is specified */
		if (CLI_PRESENT == cli_present("CMPLVL"))
		{
			if (!cli_get_int("CMPLVL", &gtmsource_options.cmplvl))
			{
				util_out_print("Error parsing CMPLVL qualifier", TRUE);
				return(-1);
			}
			if (YDB_CMPLVL_OUT_OF_RANGE(gtmsource_options.cmplvl))
				gtmsource_options.cmplvl = ZLIB_CMPLVL_MIN;	/* no compression in this case */
			/* CMPLVL qualifier should override any value specified in the environment variable ydb_zlib_cmp_level */
			ydb_zlib_cmp_level = gtmsource_options.cmplvl;
		} else
			gtmsource_options.cmplvl = ZLIB_CMPLVL_MIN;	/* no compression in this case */
		/* Check if SSL/TLS secure communication is requested. */
#		ifdef GTM_TLS
		if (CLI_PRESENT == cli_present("TLSID"))
		{
			tlsid_len = MAX_TLSID_LEN;
			if (!cli_get_str("TLSID", repl_tls.id, &tlsid_len))
			{
				util_out_print("Error parsing TLSID qualifier", TRUE);
				return -1;
			}
			assert(0 < tlsid_len);
			/* Note that "renegotiate_interval" variable units is in "minutes" below whereas
			 * "gtmsource_options.renegotiate_interval" units is in "seconds".
			 */
			if (CLI_PRESENT == cli_present("RENEGOTIATE_INTERVAL"))
			{
				if (!cli_get_int("RENEGOTIATE_INTERVAL", &renegotiate_interval))
				{
					util_out_print("Error parsing RENEGOTIATE_INTERVAL qualifier", TRUE);
					return -1;
				}
				if (0 > renegotiate_interval)
				{
					util_out_print("Negative values are not allowed for RENEGOTIATE_INTERVAL qualifier", TRUE);
					return -1;
				} else if ((0 < renegotiate_interval) && (renegotiate_interval < MIN_RENEGOTIATE_TIMEOUT))
					renegotiate_interval = MIN_RENEGOTIATE_TIMEOUT;
				if (gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD] > renegotiate_interval * 60)
				{	/* MUPIPERR when -CONNECTPARAMS having HEARTBEAT_PERIOD > renegotiate_interval */
					util_out_print("RENEGOTIATE_INTERVAL [!UL] (in minutes) cannot be less than "
							"HEARTBEAT_PERIOD [!UL]", TRUE, renegotiate_interval,
							gtmsource_options.connect_parms[GTMSOURCE_CONN_HEARTBEAT_PERIOD]);
					return -1;
				}

			} else
				renegotiate_interval = DEFAULT_RENEGOTIATE_TIMEOUT; /* Still in minutes */
			gtmsource_options.renegotiate_interval = renegotiate_interval * 60;	/* Convert to seconds */
			/* Check if plaintext-fallback mode is specified. Default option is NOPLAINTEXTFALLBACK. */
			if (CLI_PRESENT == (plaintext_fallback = cli_present("PLAINTEXTFALLBACK")))
				repl_tls.plaintext_fallback = (plaintext_fallback != CLI_NEGATED);
			else
				repl_tls.plaintext_fallback = FALSE;
			}
#		endif
	}
	if (gtmsource_options.shut_down)
	{
		if ((timeout_status = cli_present("TIMEOUT")) == CLI_PRESENT)
		{
			if (!cli_get_int("TIMEOUT", &gtmsource_options.shutdown_time))
			{
				util_out_print("Error parsing TIMEOUT qualifier", TRUE);
				return(-1);
			}
			if (MAX_SHUTDOWN_TIMEOUT < gtmsource_options.shutdown_time || 0 > gtmsource_options.shutdown_time)
			{
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVSHUTDOWN);
				return (-1);
			}
		} else if (CLI_NEGATED == timeout_status)
			gtmsource_options.shutdown_time = -1;
		else /* TIMEOUT not specified */
			gtmsource_options.shutdown_time = DEFAULT_SHUTDOWN_TIMEOUT;
	}
	if (gtmsource_options.statslog)
	{
		statslog_val_len = SIZEOF(statslog_val) - 1;
		if (!cli_get_str("STATSLOG", statslog_val, &statslog_val_len))
		{
			util_out_print("Error parsing STATSLOG qualifier", TRUE);
			return(-1);
		}
		statslog_val[statslog_val_len] = '\0';
		cli_strupper(statslog_val);
		if (0 == STRCMP(statslog_val, "ON"))
			gtmsource_options.statslog = TRUE;
		else if (0 == STRCMP(statslog_val, "OFF"))
			gtmsource_options.statslog = FALSE;
		else
		{
			util_out_print("Invalid value for STATSLOG qualifier, must be either ON or OFF", TRUE);
			return(-1);
		}
	}
	if (cli_present("FREEZE") == CLI_PRESENT)
	{
		freeze_val_len = SIZEOF(freeze_val) - 1;
		if (cli_get_str("FREEZE", freeze_val, &freeze_val_len))
		{
			gtmsource_options.setfreeze = TRUE;
			freeze_val[freeze_val_len] = '\0';
			cli_strupper(freeze_val);
			if (0 == STRCMP(freeze_val, "ON"))
				gtmsource_options.freezeval = TRUE;
			else if (0 == STRCMP(freeze_val, "OFF"))
				gtmsource_options.freezeval = FALSE;
			else
			{
				util_out_print("Invalid value for FREEZE qualifier, must be either ON or OFF", TRUE);
				return -1;
			}
			if (cli_present("COMMENT") == CLI_PRESENT)
			{
				if (!gtmsource_options.freezeval)
				{
					util_out_print("Invalid qualifier combination, cannot specify FREEZE=OFF with COMMENT",
							TRUE);
					return -1;
				}
				freeze_comment_len = SIZEOF(freeze_comment);
				if (!cli_get_str("COMMENT", freeze_comment, &freeze_comment_len))
				{
					util_out_print("Error parsing COMMENT qualifier", TRUE);
					return -1;
				}
				gtmsource_options.setcomment = TRUE;
				STRNCPY_STR(gtmsource_options.freeze_comment, freeze_comment,
					SIZEOF(gtmsource_options.freeze_comment) - 1);
				gtmsource_options.freeze_comment[SIZEOF(gtmsource_options.freeze_comment) - 1] = '\0';
			}
			else if (cli_present("COMMENT") == CLI_NEGATED)
			{
				gtmsource_options.setcomment = TRUE;
				gtmsource_options.freeze_comment[0] = '\0';
			}
			else if (gtmsource_options.freezeval)
			{
				util_out_print("Missing qualifier, must specify either COMMENT or NOCOMMENT with FREEZE=ON", TRUE);
				return -1;
			}
		}
		else
			gtmsource_options.showfreeze = TRUE;
	}
	gtmsource_options.jnlfileonly = (CLI_PRESENT == cli_present("JNLFILEONLY"));
	return(0);
}
