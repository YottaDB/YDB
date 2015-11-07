/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "cmd_qlf.h"
#include "compiler.h"
#include "opcode.h"
#include "cgp.h"
#include "io.h"
#include "list_file.h"
#include "gtmmsg.h"
#include "util_format.h"
#include "show_source_line.h"
#ifdef UNICODE_SUPPORTED
#include "gtm_utf8.h"
#endif

GBLREF char 			source_file_name[];
GBLREF unsigned char 		*source_buffer;
GBLREF short int 		source_name_len, source_line;
GBLREF command_qualifier	cmd_qlf;
GBLREF bool 			dec_nofac;
GBLREF boolean_t		run_time;
GBLREF char			cg_phase;
GBLREF io_pair			io_curr_device, io_std_device;

error_def(ERR_ACTLSTTOOLONG);
error_def(ERR_BADCASECODE);
error_def(ERR_BADCHAR);
error_def(ERR_BADCHSET);
error_def(ERR_CEBIGSKIP);
error_def(ERR_CENOINDIR);
error_def(ERR_CETOOLONG);
error_def(ERR_CETOOMANY);
error_def(ERR_CEUSRERROR);
error_def(ERR_FMLLSTMISSING);
error_def(ERR_FMLLSTPRESENT);
error_def(ERR_FOROFLOW);
error_def(ERR_INVDLRCVAL);
error_def(ERR_LABELMISSING);
error_def(ERR_SRCLIN);
error_def(ERR_SRCLOC);
error_def(ERR_SRCNAM);

void stx_error(int in_error, ...)
{
	va_list		args;
	VA_ARG_TYPE	cnt, arg1, arg2, arg3, arg4;
	bool		list, warn;
	char		msgbuf[MAX_SRCLINE];
	char		buf[MAX_SRCLINE + LISTTAB + SIZEOF(ARROW)];
	char		*c;
	mstr		msg;
	boolean_t	is_stx_warn;	/* current error is actually of type warning and we are in CGP_PARSE phase */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	va_start(args, in_error);
	/* In case of a IS_STX_WARN type of parsing error, we resume parsing so it is important NOT to reset
	 * the following global variables
	 *	a) saw_side_effect
	 *	b) shift_side_effects
	 *	c) source_error_found
	 */
	is_stx_warn = (CGP_PARSE == cg_phase) && IS_STX_WARN(in_error) GTMTRIG_ONLY( && !TREF(trigger_compile));
	if (!is_stx_warn)
		TREF(saw_side_effect) = TREF(shift_side_effects) = FALSE;
	if (run_time)
	{	/* If the current error is of type STX_WARN then do not issue an error at compile time. Insert
		 * triple to issue the error at runtime. If and when this codepath is reached at runtime (M command
		 * could have a postconditional that bypasses this code) issue the rts_error.
		 * See IS_STX_WARN macro definition for details.
		 */
		if (is_stx_warn)
		{	/* merrors.msg defines INVCMD as a warning but compiler conditions can turn it into an error */
			if (ERR_INVCMD != in_error)	/* if INVCMD has morphed into an error, it won't match here */
				ins_errtriple(in_error);
			return;
		}
		if (TREF(for_stack_ptr) > (oprtype **)TADR(for_stack))
			FOR_POP(BLOWN_FOR);
		if (ERR_BADCHAR == in_error)
		{
			cnt = va_arg(args, VA_ARG_TYPE);
			assert(cnt == 4);
			arg1 = va_arg(args, VA_ARG_TYPE);
			arg2 = va_arg(args, VA_ARG_TYPE);
			arg3 = va_arg(args, VA_ARG_TYPE);
			arg4 = va_arg(args, VA_ARG_TYPE);
			va_end(args);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(6) in_error, cnt, arg1, arg2, arg3, arg4);
		} else if ((ERR_LABELMISSING == in_error)
			|| (ERR_FMLLSTMISSING == in_error)
			|| (ERR_ACTLSTTOOLONG == in_error)
			|| (ERR_BADCHSET == in_error)
			|| (ERR_BADCASECODE == in_error))
		{
			cnt = va_arg(args, VA_ARG_TYPE);
			assert(cnt == 2);
			arg1 = va_arg(args, VA_ARG_TYPE);
			arg2 = va_arg(args, VA_ARG_TYPE);
			va_end(args);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) in_error, cnt, arg1, arg2);
		} else if ((ERR_CEUSRERROR == in_error) || (ERR_INVDLRCVAL == in_error) || (ERR_FOROFLOW == in_error))
		{
			cnt = va_arg(args, VA_ARG_TYPE);
			assert(cnt == 1);
			arg1 = va_arg(args, VA_ARG_TYPE);
			va_end(args);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) in_error, cnt, arg1);
		} else
		{
			va_end(args);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) in_error);
		}
	} else if ((CGP_PARSE == cg_phase) && (ERR_INVCMD != in_error))	/* if INVCMD has morphed into an error, won't match here */
		ins_errtriple(in_error);
	assert(!run_time);	/* From here on down, should never go ahead with printing compile-error while in run_time */
	flush_pio();
	if (TREF(source_error_found))
	{
		va_end(args);
		return;
	}
	if (!is_stx_warn
		&& (ERR_CETOOMANY != in_error)	/* compiler escape errors shouldn't hide others */
		&& (ERR_CEUSRERROR!= in_error)
		&& (ERR_CEBIGSKIP != in_error)
		&& (ERR_CETOOLONG != in_error)
		&& (ERR_CENOINDIR != in_error))
	{
		TREF(source_error_found) = (int4)in_error;
	}
	list = (cmd_qlf.qlf & CQ_LIST) != 0;
	warn = (cmd_qlf.qlf & CQ_WARNINGS) != 0;
	if (!warn && !list) /*SHOULD BE MESSAGE TYPE IS WARNING OR LESS*/
	{
		va_end(args);
		return;
	}
	if (list && (io_curr_device.out == io_std_device.out))
		warn = FALSE;		/* if listing is going to $P, don't double output */
	if (ERR_BADCHAR == in_error)
	{
		show_source_line(warn);
		cnt = va_arg(args, VA_ARG_TYPE);
		assert(cnt == 4);
		arg1 = va_arg(args, VA_ARG_TYPE);
		arg2 = va_arg(args, VA_ARG_TYPE);
		arg3 = va_arg(args, VA_ARG_TYPE);
		arg4 = va_arg(args, VA_ARG_TYPE);
		if (warn)
		{
			dec_err(VARLSTCNT(6) in_error, 4, arg1, arg2, arg3, arg4);
			dec_err(VARLSTCNT(4) ERR_SRCNAM, 2, source_name_len, source_file_name);
		}
		arg1 = arg2 = arg3 = arg4 = 0;
	} else if ((ERR_LABELMISSING == in_error)
		|| (ERR_FMLLSTMISSING == in_error)
		|| (ERR_ACTLSTTOOLONG == in_error)
		|| (ERR_BADCHSET == in_error)
		|| (ERR_BADCASECODE == in_error))
	{
		cnt = va_arg(args, VA_ARG_TYPE);
		assert(cnt == 2);
		arg1 = va_arg(args, VA_ARG_TYPE);
		arg2 = va_arg(args, VA_ARG_TYPE);
		if (warn)
		{
			dec_err(VARLSTCNT(4) in_error, 2, arg1, arg2);
			dec_err(VARLSTCNT(4) ERR_SRCNAM, 2, source_name_len, source_file_name);
		}
	} else
	{
		show_source_line(warn);
		if (warn)
		{
			if ((ERR_CEUSRERROR != in_error) && (ERR_INVDLRCVAL != in_error) && (ERR_FOROFLOW != in_error))
				dec_err(VARLSTCNT(1) in_error);
			else
			{
				cnt = va_arg(args, VA_ARG_TYPE);
				assert(cnt == 1);
				arg1 = va_arg(args, VA_ARG_TYPE);
				dec_err(VARLSTCNT(3) in_error, 1, arg1);
			}
		}
		arg1 = arg2 = 0;
	}
	va_end(args);
	if (list)
	{
		va_start(args, in_error);
		msg.addr = msgbuf;
		msg.len = SIZEOF(msgbuf);
		gtm_getmsg(in_error, &msg);
		assert(msg.len);
#		ifdef UNIX
		cnt = va_arg(args, VA_ARG_TYPE);
		c = util_format(msgbuf, args, LIT_AND_LEN(buf), (int)cnt);
		va_end(TREF(last_va_list_ptr));	/* set by util_format */
#		else
		c = util_format(msgbuf, args, LIT_AND_LEN(buf));
#		endif
		va_end(args);
		*c = 0;
		list_line(buf);
	}
}
