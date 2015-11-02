/****************************************************************
 *								*
 *	Copyright 2001, 2010 Fidelity Information Services Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 *  gtcm_prsopt.c ---
 *
 *	This routine parses up the GTCM server's command line and
 *	sets various global flags accordingly.
 *
 */

#include "mdef.h"

#include "gtm_string.h"


#include "gtm_stdlib.h"		/* for exit() */
#include "gtm_stdio.h"
#ifdef __MVS__
#include "eintr_wrappers.h"
#include "gtm_stat.h"
#include "gtm_zos_io.h"
#endif
#include "gtcm.h"

#ifndef lint
static char rcsid[] = "$Header:$";
#endif

GBLREF FILE	*omi_debug;
GBLREF int	 omi_pkdbg;
GBLREF char	*omi_pklog;
GBLREF char	*omi_service;
GBLREF int	 rc_server_id;
GBLREF char	*omi_pklog_addr;
GBLREF int	one_conn_per_inaddr;
GBLREF int	authenticate;
GBLREF int	ping_keepalive;
GBLREF int	conn_timeout;
GBLREF int	history;
GBLDEF int	per_conn_servtime = MIN_TIMEOUT_INTERVAL;

enum opt_enum
{
	opt_null, opt_debug, opt_pktlog, opt_service, opt_rc_id,
	opt_pktlog_addr, opt_authenticate, opt_multipleconn,
	opt_ping, opt_conn_timeout, opt_history, opt_servtime
};

static struct
{
  char *name;
  enum opt_enum option;
  int args;
} optlist[] =
{
	"-D",		opt_debug, 1,	/* debugging output */
	"-log",		opt_debug, 1,
	"-P",		opt_pktlog, 1,	/* packet log file template */
	"-pktlog",	opt_pktlog, 1,
	"-S",		opt_service, 1,	/* service name in /etc/services */
	"-service",	opt_service, 1,
	"-I",		opt_rc_id, 1,	/* RC server ID */
	"-id",		opt_rc_id, 1,
	"-A",		opt_pktlog_addr, 1, /* IP address of DT agent to log */
	"-logaddr",	opt_pktlog_addr, 1,
	"-auth",	opt_authenticate, 0, /* authenticate connections */
	"-multiple",	opt_multipleconn,  0, /* allow multiple conn from same IP address */
	"-ping",	opt_ping, 0,	/* ping connections to keepalive */
	"-timeout",	opt_conn_timeout, 1,
	"-servtime",	opt_servtime,	1, /* Used for setup the timer for servicing each connection, default 60 sec */
	"-hist",	opt_history, 0, /* flag:  keep packet history in mem */
	NULL,		opt_null, 0
};

/* On OSF/1 (Digital Unix), pointers are 64 bits wide; the only exception to this is C programs for which one may
 * specify compiler and link editor options in order to use (and allocate) 32-bit pointers.  However, since C is
 * the only exception and, in particular because the operating system does not support such an exception, the argv
 * array passed to the main program is an array of 64-bit pointers.  Thus the C program needs to declare argv[]
 * as an array of 64-bit pointers and needs to do the same for any pointer it sets to an element of argv[].
 */
int gtcm_prsopt(int argc, char_ptr_t argv[])
{
    enum opt_enum opt;
    int	 i,j, t;
    boolean_t inv_option = FALSE;

    for (i = 1, argv++; i < argc; argv += optlist[j].args + 1, i += optlist[j].args + 1)
    {
	    for(j = 0; opt = optlist[j].option; j++)
		    if (!strcmp(*argv,optlist[j].name))
			    break;
	    if (i + optlist[j].args >= argc)
	    {
		    FPRINTF(stderr, "%s option requires an argument - ignored\n", *argv);
		    continue;
	    }
	    switch(opt)
	    {
		  case opt_debug:
		    if ((*(argv + 1))[0] == '-' && (*(argv + 1))[1] == '\0')
			    omi_debug = stdout;
		    else if ((*(argv + 1))[0] == '=' && (*(argv + 1))[1] == '\0')
			    omi_debug = stderr;
		    else
		    {
#ifdef __MVS__
			    if (-1 == gtm_zos_create_tagged_file(*(argv + 1), TAG_EBCDIC))
					perror("error tagging log file");
#endif
		    	    if (!(omi_debug = fopen(*(argv + 1), "w+")))
		    	    {
			    	    perror("error opening log file");
			    	    exit(1);
		    	    }
		    }
		    break;
		  case opt_pktlog:	omi_pklog = *(argv + 1);  break;
		  case opt_service:	omi_service = *(argv + 1); break;
		  case opt_rc_id:	rc_server_id = atoi(*(argv + 1)); break;
		  case opt_pktlog_addr: omi_pklog_addr = *(argv + 1); break;
		  case opt_authenticate: authenticate = 1; break;
		  case opt_multipleconn:
		    one_conn_per_inaddr = 0;
		    break;
		  case opt_ping: 	ping_keepalive = 1; break;
		  case opt_null:
		    inv_option = TRUE;
		    FPRINTF(stderr,"Unknown option:  %s\n",*argv);
		    break;
		  case opt_conn_timeout:
		    t = atoi(*(argv + 1));
		    if (t < MIN_TIMEOUT_INTERVAL)
			FPRINTF(stderr,"-timeout parameter must be >= %d seconds. The default value %d seconds will be used\n"
				, MIN_TIMEOUT_INTERVAL, TIMEOUT_INTERVAL);
		    else
			    conn_timeout = t;
		    break;
		  case opt_servtime:
		    t = atoi(*(argv + 1));
		    if (t < MIN_TIMEOUT_INTERVAL)
			  FPRINTF(stderr, "-servtime parameter must be >= %d seconds. The default value %d seconds will be used\n",
				    MIN_TIMEOUT_INTERVAL, MIN_TIMEOUT_INTERVAL);
		    else
			    per_conn_servtime = t;
		    break;
		  case opt_history:
		    history = 1;
		    break;
		  default:
			inv_option = TRUE;
		    FPRINTF(stderr,"Unsupported option:  %s\n",*argv);
		    break;
	    }
	    if (inv_option)
	    	return -1;
    }
    return 0;
}
