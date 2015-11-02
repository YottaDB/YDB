/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stdio.h"

#include "gtm_event_log.h"
#include <rtnhdr.h>
#include "lv_val.h"	/* needed for "fgncal.h" */
#include "fgncal.h"
#include "io.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "trans_log_name.h"
#include "error.h"
#include "gtm_logicals.h"

#define MAXARGSIZE	1024

static boolean_t	gtm_do_event_log = FALSE;
static fgnfnc		gtm_event_log_func;
static void_ptr_t	gtm_event_log_handle;

int gtm_event_log_init(void)
{
	/* External log initializations */

	mstr	name, trans_name;
	char    log_name[MAX_TRANS_NAME_LEN],
		shared_lib[MAX_TRANS_NAME_LEN],
		log_func[MAX_TRANS_NAME_LEN];
	int	status, index, save_errno;
	char	print_msg[1024], *args;

	error_def(ERR_EVENTLOGERR);
	error_def(ERR_TEXT);

	if (gtm_do_event_log) /* Already initialized */
		return(SS_NORMAL);
	name.len = SIZEOF(GTM_EVENT_LOG_LIB_ENV) - 1;
	name.addr = GTM_EVENT_LOG_LIB_ENV;
	if (SS_NORMAL != (status = TRANS_LOG_NAME(&name, &trans_name, log_name, SIZEOF(log_name), do_sendmsg_on_log2long))
			|| (0 == trans_name.len))
		return(status);
	memcpy(shared_lib, trans_name.addr, trans_name.len);
	shared_lib[trans_name.len] = '\0';
	if (NULL == (gtm_event_log_handle = fgn_getpak(shared_lib, INFO)))
	{
		SPRINTF(print_msg, "Could not open shared library specified in %s - %s. No event logging done",
			GTM_EVENT_LOG_LIB_ENV, shared_lib);
		gtm_putmsg(VARLSTCNT(6) ERR_EVENTLOGERR, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
		return(-1);
	}
#ifdef GTM_EVENT_LOG_HARDCODE_RTN_NAME
	trans_name.len = SIZEOF(GTM_EVENT_LOG_RTN) - 1;
	trans_name.addr = GTM_EVENT_LOG_RTN;
#else
	name.len = SIZEOF(GTM_EVENT_LOG_RTN_ENV) - 1;
	name.addr = GTM_EVENT_LOG_RTN_ENV;
	if (SS_NORMAL != (status = TRANS_LOG_NAME(&name, &trans_name, log_name, SIZEOF(log_name), do_sendmsg_on_log2long))
		|| (0 == trans_name.len))
	{
		SPRINTF(print_msg, "%s not set or null. No event logging done", GTM_EVENT_LOG_RTN_ENV);
		gtm_putmsg(VARLSTCNT(6) ERR_EVENTLOGERR, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
		return(status);
	}
#endif
	memcpy(log_func, trans_name.addr, trans_name.len);
	log_func[trans_name.len] = '\0';

	if (NULL == (gtm_event_log_func = fgn_getrtn(gtm_event_log_handle, &trans_name, INFO)))
	{
#ifdef GTM_EVENT_LOG_HARDCODE_RTN_NAME
		SPRINTF(print_msg, "Could not find function %s in shared library %s. No event logging done",
			log_func, shared_lib);
#else
		SPRINTF(print_msg,
			"Could not find function specified in %s - %s in shared library %s. No event logging done",
			GTM_EVENT_LOG_RTN_ENV, log_func, shared_lib);
#endif
		gtm_putmsg(VARLSTCNT(6) ERR_EVENTLOGERR, 0, ERR_TEXT, 2, LEN_AND_STR(print_msg));
		return(-1);
	}

	gtm_do_event_log = TRUE;
	return(SS_NORMAL);
}

int gtm_event_log_close(void)
{
	if (gtm_do_event_log)
		fgn_closepak(gtm_event_log_handle, INFO);
	gtm_do_event_log = FALSE;
	return(SS_NORMAL);
}

int gtm_event_log(int argc, char *category, char *code, char *msg)
{
	if (gtm_do_event_log)
		gtm_event_log_func(argc, category, code, msg);
	return(SS_NORMAL);
}
