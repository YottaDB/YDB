/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "stringpool.h"
#include <descrip.h>
#include "get_command_line.h"

#define MAX_COMMAND_LINE_LENGTH 255

GBLREF spdesc		stringpool;

void get_command_line(mval *v, boolean_t zcmd_line)
{
	struct dsc$descriptor_s cmd_desc;

	/* zcmd_line is currently unused in VMS */
	ENSURE_STP_FREE_SPACE(MAX_COMMAND_LINE_LENGTH);
	v->mvtype = MV_STR;
	cmd_desc.dsc$w_length = MAX_COMMAND_LINE_LENGTH;
	cmd_desc.dsc$b_dtype = DSC$K_DTYPE_T;
	cmd_desc.dsc$b_class = DSC$K_CLASS_S;
	cmd_desc.dsc$a_pointer = stringpool.free;
	/* initialize the 4-byte int v->str.len to 0 before calling lib$get_foreign() with &v->str.len as the function
	 * expects a pointer to a short (rather than an int) and hence will reset only the lower-order 2-bytes in v->str.len
	 */
	v->str.len = 0;
	if ((lib$get_foreign (&cmd_desc, 0, &v->str.len) & 1) == 0)
		v->str.len = 0;
	v->str.addr = stringpool.free;
	stringpool.free += v->str.len;
	assert(stringpool.free >= stringpool.base);
	assert(stringpool.free < stringpool.top);
	return;
}
