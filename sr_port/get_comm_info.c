/****************************************************************
 *								*
 * Copyright (c) 2020-2021 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#include "gtm_stdio.h"
#include "gtm_string.h"
#include <errno.h>

#include "get_comm_info.h"
#include "gtm_caseconv.h"
#include "eintr_wrappers.h"  /* For FCLOSE definition */
#include "util.h"

/* Function get_com_info() reads the /proc/self/comm file to get the executable name that
 * the process was started with.
 */
void get_comm_info(void)
{
	char	*commfilepath= "/proc/self/comm";
	char	*process_rs;
	int	rc, process_name_len;
	FILE	*fp;

	Fopen(fp, commfilepath, "r");
	if (NULL == fp)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fopen()"), CALLFROM, errno);
	}
	FGETS(process_name, PROCESS_NAME_LENGTH, fp, process_rs); /* process_name is the name of the current running ydb process*/
	if (NULL == process_rs)
	{
		FCLOSE(fp, rc);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fgets()"), CALLFROM, errno);
	}
	process_name_len = STRLEN(process_name); /* We know file was read */
	assertpro(PROCESS_NAME_LENGTH > process_name_len);
	FCLOSE(fp, rc);
	if (0 != rc)
	{
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("fclose()"), CALLFROM, rc);
	}
	if (1 < process_name_len) /* Name beyond terminating name character */
	{
		process_name_len--;
		assert('\n' == *(process_name + process_name_len));
		process_name[process_name_len] = '\0'; /* Removing trailing line-end character */
		lower_to_upper((uchar_ptr_t)process_name, (uchar_ptr_t)process_name, (int4)process_name_len);
	}
}
