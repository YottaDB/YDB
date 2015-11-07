/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"
#include "gtm_stdlib.h"
#include "gtm_time.h"
#include "gtm_stdio.h"

#include "repl_log.h"
#include "iosp.h"
#include "util.h"

#define MAX_MSG_LEN 1024

GBLREF int		gtmsource_log_fd;
GBLREF int		gtmrecv_log_fd;
GBLREF int		updproc_log_fd;

GBLREF FILE		*gtmsource_log_fp;
GBLREF FILE		*gtmrecv_log_fp;
GBLREF FILE		*updproc_log_fp;

/* Note :  On VMS 'fp' argument is dummy. Message is always logged to the log file specified by the
   /LOG qualifier, or to the stdout otherwise */

void map_esc_seq(char *in_msg)
{
	char	out_msg[MAX_MSG_LEN];
	char	*out_ptr, *in_ptr;

	in_ptr  = in_msg;
	out_ptr = out_msg;
	while('\0' != *in_ptr)
	{
		switch(*in_ptr)
		{
			case '\n': *out_ptr++ = '!';
				   *out_ptr++ = '/';
				   break;
			case '\t': *out_ptr++ = '!';
				   *out_ptr++ = '_';
				   break;
			case '\f': *out_ptr++ = '!';
				   *out_ptr++ = '^';
				   break;
			case '!':  *out_ptr++ = '!';
				   *out_ptr++ = '!';
				   break;
			default :  *out_ptr++ = *in_ptr;
		}
		in_ptr++;
	}
	*out_ptr = '\0';
	strcpy(in_msg, out_msg);
}

int repl_log(FILE *fp, boolean_t stamptime, boolean_t flush, char *fmt, ...)
{
	va_list printargs;
	now_t	now; /* for GET_CUR_TIME macro */
	char	*time_ptr, time_str[CTIME_BEFORE_NL + 2]; /* for GET_CUR_TIME macro */
	char	fmt_str[BUFSIZ];
	char	msg[MAX_MSG_LEN];
	int	msg_len, rc;

	if (stamptime)
	{
		GET_CUR_TIME;
		strcpy(fmt_str, time_ptr);
		fmt_str[CTIME_BEFORE_NL] = ' '; /* Overwrite \n */
		fmt_str[CTIME_BEFORE_NL + 1] = ':';
		fmt_str[CTIME_BEFORE_NL + 2] = ' ';
		strcpy(fmt_str + CTIME_BEFORE_NL + 3, fmt);
		fmt = &fmt_str[0];
	}

	va_start(printargs, fmt);
	VSPRINTF(msg, fmt, printargs, rc);
	va_end(printargs);
	msg_len = strlen(msg);
	if (flush && '\n' == msg[msg_len - 1])
		msg[msg_len - 1] = '\0'; /* Since the util_out_print() puts a CR by itself */
	map_esc_seq(msg);
	util_out_print(msg, flush);

	return(SS_NORMAL);
}
