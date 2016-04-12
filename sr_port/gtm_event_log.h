/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GTM_EVENT_LOG_H__
#define __GTM_EVENT_LOG_H__
/* GTM external logging */

#define GTM_EVENT_LOG_HARDCODE_RTN_NAME	/* undef this to use env variable */

#ifdef GTM_EVENT_LOG_HARDCODE_RTN_NAME
#	define GTM_EVENT_LOG_RTN		"GtmEventLog"
extern	int	GtmEventLog(int argc, char *category, char *code, char *msg);
#endif

#define GTM_EVENT_LOG_ARGC	3 /* excluding the argument count - category, code and msg */

int gtm_event_log_init(void);
int gtm_event_log_close(void);
int gtm_event_log(int argc, char *category, char *code, char *msg);

#endif
