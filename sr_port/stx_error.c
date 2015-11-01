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
#include <varargs.h>
#include "gtm_string.h"
#include "cmd_qlf.h"
#include "compiler.h"
#include "cgp.h"
#include "io.h"
#include "list_file.h"
#include "gtmmsg.h"
#include "util_format.h"

GBLREF char 			source_file_name[];
GBLREF unsigned char 		*source_buffer;
GBLREF short int 		source_name_len, last_source_column, source_line;
GBLREF int4 			source_error_found;
GBLREF command_qualifier	cmd_qlf;
GBLREF bool 			shift_gvrefs, run_time, dec_nofac;
GBLREF char			cg_phase;
GBLREF io_pair			io_curr_device, io_std_device;

void stx_error(va_alist)
va_dcl
{
	va_list	args, sav_arg;
	int	in_error, cnt, arg1, arg2;
	bool	list, warn;
	char	msgbuf[MAX_SRCLINE];
	char	buf[MAX_SRCLINE + LISTTAB + 7];
	char	*b, *c, *b_top, *c_top;
	mstr	msg;
	error_def(ERR_SRCLIN);
	error_def(ERR_SRCLOC);
	error_def(ERR_SRCNAM);
	error_def(ERR_LABELMISSING);
	error_def(ERR_FMLLSTPRESENT);
	error_def(ERR_FMLLSTMISSING);
	error_def(ERR_ACTLSTTOOLONG);
	error_def(ERR_CETOOMANY);
	error_def(ERR_CEUSRERROR);
	error_def(ERR_CEBIGSKIP);
	error_def(ERR_CETOOLONG);
	error_def(ERR_CENOINDIR);

	flush_pio();
	va_start(args);
	in_error = va_arg(args, int);
	shift_gvrefs = FALSE;
	if (run_time)
	{
		if (in_error == ERR_LABELMISSING ||
		    in_error == ERR_FMLLSTPRESENT ||
		    in_error == ERR_FMLLSTMISSING ||
		    in_error == ERR_ACTLSTTOOLONG)
		{
			cnt = va_arg(args, int);
			assert(cnt == 2);
			arg1 = va_arg(args, int);
			arg2 = va_arg(args, int);
			rts_error(VARLSTCNT(4) in_error, cnt, arg1, arg2);
		}
		else if (in_error == ERR_CEUSRERROR)
		{
			cnt = va_arg(args, int);
			assert(cnt == 1);
			arg1 = va_arg(args, int);
			rts_error(VARLSTCNT(3) in_error, cnt, arg1);
		}
		else
		{	rts_error(VARLSTCNT(1) in_error);
		}
		GTMASSERT;
	}
	else if (cg_phase == CGP_PARSE)
	{	ins_errtriple(in_error);
	}
	if (source_error_found)
	{	return;
	}
	if (in_error != ERR_CETOOMANY &&	/* compiler escape errors shouldn't hide others */
	    in_error != ERR_CEUSRERROR &&
	    in_error != ERR_CEBIGSKIP &&
	    in_error != ERR_CETOOLONG &&
	    in_error != ERR_CENOINDIR)
	{
		source_error_found = (int4 ) in_error;
	}
	list = (cmd_qlf.qlf & CQ_LIST) != 0;
	warn = (cmd_qlf.qlf & CQ_WARNINGS) != 0;
	if (!warn && !list) /*SHOULD BE MESSAGE TYPE IS WARNING OR LESS*/
	{	return;
	}

	if (list && io_curr_device.out == io_std_device.out)
		warn = FALSE;		/* if listing is going to $P, don't double output */

	if (in_error != ERR_LABELMISSING &&
	    in_error != ERR_FMLLSTPRESENT &&
	    in_error != ERR_FMLLSTMISSING &&
	    in_error != ERR_ACTLSTTOOLONG)
	{
		memset(buf, ' ', LISTTAB);

		for (c = (char *)source_buffer, b = &buf[LISTTAB], b_top = b + sizeof buf - 6, c_top = c + last_source_column - 1;
			c < c_top && b < b_top; c++ )
			if (*c == '\t')
				*b++ = '\t';
			else
				*b++ = ' ';
		memcpy(b, "^-----",6);
		b += 6;
		*b = 0;

		if (warn)
		{
			dec_nofac = TRUE;
			dec_err(VARLSTCNT(6) ERR_SRCLIN, 4, LEN_AND_STR((char *)source_buffer), b - &buf[LISTTAB], &buf[LISTTAB]);
			dec_err(VARLSTCNT(6) ERR_SRCLOC, 4, last_source_column, source_line, source_name_len, source_file_name);
			dec_nofac = FALSE;
			if (in_error != ERR_CEUSRERROR)
			{
				dec_err(VARLSTCNT(1) in_error);
			}
			else
			{
				cnt = va_arg(args, int);
				assert(cnt == 1);
				arg1 = va_arg(args, int);
				dec_err(VARLSTCNT(3) in_error, 1, arg1);
			}
		}
		if (list)
		{	list_line(buf);
		}
		arg1 = arg2 = 0;
	}
	else
	{
		cnt = va_arg(args, int);
		assert(cnt == 2);
		VAR_COPY(sav_arg, args);
		arg1 = va_arg(args, int);
		arg2 = va_arg(args, int);
		if (warn)
		{
			dec_err(VARLSTCNT(4) in_error, 2, arg1, arg2);
			dec_err(VARLSTCNT(4) ERR_SRCNAM, 2, source_name_len, source_file_name);
		}
	}
	if (list)
	{
		msg.addr = msgbuf;
		msg.len = sizeof(msgbuf);
		gtm_getmsg(in_error, &msg);
		assert(msg.len);
#ifdef UNIX
		c = util_format(msgbuf, sav_arg, LIT_AND_LEN(buf), MAXPOSINT4);
#else
		c = util_format(msgbuf, sav_arg, LIT_AND_LEN(buf));
#endif
		*c = 0;
		list_line(buf);
	}
}
