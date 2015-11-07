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
#include <jpidef.h>
#include <ssdef.h>
#include <descrip.h>
#include "getzmode.h"

#define MAX_MODE_LEN 15

void getzmode(void)
{
	static unsigned char mode_buf[MAX_MODE_LEN];
	static readonly int4 jpi_code = JPI$_MODE;
	$DESCRIPTOR(out_string,"");
	short int out_len;
	uint4 status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	out_string.dsc$a_pointer = mode_buf;
	out_string.dsc$w_length = MAX_MODE_LEN;
	if (SS$_NORMAL != (status = lib$getjpi(&jpi_code ,0 ,0, 0, &out_string, &out_len)))	/* Intentional assignment */
		rts_error(VARLSTCNT(1) status);
	(TREF(dollar_zmode)).str.addr = &mode_buf[0];
	(TREF(dollar_zmode)).mvtype = MV_STR;
	(TREF(dollar_zmode)).str.len = out_len;
	return;
}
