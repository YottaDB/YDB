/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_strings.h"

#include "stringpool.h"
#include "io.h"
#include <stdarg.h>
#include "op.h"

#define LIMITCHECK(x, xmax)		\
	if ((x) > (xmax))		\
	{				\
		overflow = TRUE;	\
		break;			\
	}

GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;

LITDEF MSTR_CONST(literal_accept, "ACCEPT");
LITDEF MSTR_CONST(literal_listen, "LISTEN");
LITDEF MSTR_CONST(literal_wait, "WAIT");

#define MSTR_CASE_EQ(x, y)	(((x)->len == (y)->len) && !STRNCASECMP((x)->addr, (y)->addr, (x)->len))

error_def(ERR_CTLMNEMAXLEN);

void op_iocontrol(UNIX_ONLY_COMMA(int4 n) mval *vparg, ...)
{
	va_list		var;
	VMS_ONLY(int	n;)

	VAR_START(var, vparg);
	VMS_ONLY(va_count(n);)
	assert(0 < n);
	MV_FORCE_STR(vparg);
	if (MSTR_CASE_EQ(&vparg->str, &literal_accept)
			|| MSTR_CASE_EQ(&vparg->str, &literal_listen)
			|| MSTR_CASE_EQ(&vparg->str, &literal_wait))
		(*io_curr_device.in->disp_ptr->iocontrol)(&vparg->str, n-1, var);
	else
		(*io_curr_device.out->disp_ptr->iocontrol)(&vparg->str, n-1, var);
	va_end(var);
	return;
}
