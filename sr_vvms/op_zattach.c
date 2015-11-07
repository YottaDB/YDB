/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_limits.h"
#include "io.h"
#include <descrip.h>
#include <ssdef.h>
#include <jpidef.h>
#include "gtm_caseconv.h"
#include "setterm.h"
#include "op.h"
#include "min_max.h"

#define PID_BUFF 15
GBLREF io_pair		io_std_device;
GBLREF mval 		dollar_zproc;
static int4		parent_id;
static bool		once_thru = FALSE;

void op_zattach(mval *v)
{
	int4	pid;
	int4 	jpi_code;
	uint4 	status;
	$DESCRIPTOR(p_name,"");
	error_def(ERR_ZATTACHERR);

	MV_FORCE_STR(v);
	if (PID_BUFF < v->str.len)
		rts_error(VARLSTCNT(5) ERR_ZATTACHERR, 2, MIN(SHRT_MAX, v->str.len), v->str.addr, SS$_IVLOGNAM);
	if (once_thru == FALSE)
	{	once_thru = TRUE;

		p_name.dsc$w_length = dollar_zproc.str.len;
		p_name.dsc$a_pointer = dollar_zproc.str.addr;
		jpi_code = JPI$_MASTER_PID;

		lib$getjpi(&jpi_code, 0, &p_name, &parent_id, 0, 0);
	}

	p_name.dsc$w_length = v->str.len;
	if (!p_name.dsc$w_length)
	{	pid = parent_id;
	}
	else
	{
		jpi_code = JPI$_PID;
		p_name.dsc$a_pointer = v->str.addr;

		status = lib$getjpi(&jpi_code, 0, &p_name, &pid, 0, 0);
		if (status == SS$_NONEXPR)
		{
			char buf[PID_BUFF];

			assert (v->str.len <= PID_BUFF);
			lower_to_upper(&buf[0], v->str.addr, v->str.len);
			p_name.dsc$a_pointer = buf;
			status = lib$getjpi(&jpi_code, 0, &p_name, &pid, 0, 0);
		}
		if (status != SS$_NORMAL)
		{	rts_error(VARLSTCNT(6) ERR_ZATTACHERR, 2, v->str.len, v->str.addr, status, 0);
		}
	}
	flush_pio();
	if (io_std_device.in->type == tt)
		resetterm(io_std_device.in);
	status = lib$attach(&pid);
	if (io_std_device.in->type == tt)
		setterm(io_std_device.in);
	if (status != SS$_NORMAL)
	{	rts_error(VARLSTCNT(6) ERR_ZATTACHERR, 2, v->str.len, v->str.addr, status, 0);
	}
}
