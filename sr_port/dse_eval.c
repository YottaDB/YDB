/****************************************************************
 *								*
 * Copyright (c) 2001-2025 Fidelity National Information	*
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
#define HEX_STR		"Hex:  "
#define DEC_STR		"   Dec:  !@UQ"
error_def(ERR_CLIERR);
void dse_eval(void)
{
	int4		util_len;
	char		num_buff[MAX_LINE];
	gtm_uint64_t	num;
	char		util_buff[MAX_UTIL_LEN];
	unsigned short	len = MAX_LINE;
	char		err_buff[MAX_LINE];
	boolean_t	is_hex_req = 0, is_dec_req = 0, is_valid_dcm = 0, is_valid_hex = 0;

	if (cli_present("NUMBER") != CLI_PRESENT)
		return;
	if (!cli_get_str("NUMBER", num_buff, &len))
		return;

	is_hex_req = cli_present("HEXADECIMAL") == CLI_PRESENT;
	is_dec_req = cli_present("DECIMAL") == CLI_PRESENT;
	is_valid_dcm = cli_is_dcm_negok(num_buff);
	is_valid_hex = cli_is_hex_negok(num_buff);

	if (is_dec_req)
	{
		if (!is_valid_dcm)
		{
			SNPRINTF(err_buff, SIZEOF(err_buff), "Unrecognized value: %s, A valid decimal integer required", num_buff);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLIERR , 2, LEN_AND_STR(err_buff));
			return;
		}
		if (!cli_get_uint64("NUMBER", (gtm_uint64_t *)&num))
			return;
	} else
	{
		if (!is_valid_hex)
		{
			SNPRINTF(err_buff,SIZEOF(err_buff),
				"Unrecognized value: %s, A valid hexadecimal integer required",num_buff);
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_CLIERR , 2, LEN_AND_STR(err_buff));
			return;
		}
		if (!cli_get_hex64("NUMBER", &num))
			return;
	}

	MEMCPY_LIT(util_buff, HEX_STR);
	util_len = strlen(HEX_STR);
	util_len += i2hexl_nofill(num, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
	MEMCPY_LIT(&util_buff[util_len], DEC_STR);
	util_len += strlen(DEC_STR);
	util_buff[util_len] = 0;
	util_out_print(util_buff, TRUE, &num);

	if (!is_hex_req && !is_dec_req && is_valid_dcm)
	{
		util_len = 0;
		MEMCPY_LIT(util_buff, HEX_STR);
		util_len += strlen(HEX_STR);
		num = strtol(num_buff, NULL, 10);
		util_len += i2hexl_nofill(num, (uchar_ptr_t)&util_buff[util_len], MAX_HEX_INT8);
		MEMCPY_LIT(&util_buff[util_len], DEC_STR);
		util_len += strlen(DEC_STR);
		util_buff[util_len] = 0;
		util_out_print(util_buff, TRUE, &num);
	}
}
