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
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "cli.h"
#include "util.h"
#include "dse.h"

#define MAX_UTIL_LEN 32

void dse_eval(void)
{
	int4	num, util_len;
	char		util_buff[MAX_UTIL_LEN];

	if (cli_present("NUMBER") != CLI_PRESENT)
		return;
	if (cli_present("DECIMAL") == CLI_PRESENT)
	{	if (!cli_get_num("NUMBER",&num))
		{	return;
		}
	}else if (!cli_get_hex("NUMBER",&num))
	{	return;
	}
	memcpy(util_buff,"Hex:  ",6);
	util_len = 6;
	util_len += i2hex_nofill(num,(uchar_ptr_t)&util_buff[util_len],8);
	memcpy(&util_buff[util_len],"   Dec:  !UL",12);
	util_len += 12;
	util_buff[util_len] = 0;
	util_out_print(util_buff,TRUE,num);
	return;
}
