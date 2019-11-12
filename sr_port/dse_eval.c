/****************************************************************
 *								*
 * Copyright (c) 2001-2019 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "util.h"
#include "dse.h"

void dse_eval(void)
{
	int4		util_len;
	gtm_uint64_t	num;
	char		util_buff[MAX_UTIL_LEN];

	if (cli_present("NUMBER") != CLI_PRESENT)
		return;
	if (cli_present("DECIMAL") == CLI_PRESENT)
	{
		if (!cli_get_uint64("NUMBER", (gtm_uint64_t *)&num))
			return;
	} else if (!cli_get_hex64("NUMBER", &num))
		return;
	memcpy(util_buff, "Hex:  ", 6);
	util_len = 6;
	util_len += i2hexl_nofill(num, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
	memcpy(&util_buff[util_len],"   Dec:  !@UQ", 13);
	util_len += 13;
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE, &num);
	return;
}
