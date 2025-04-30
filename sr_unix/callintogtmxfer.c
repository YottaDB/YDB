/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ***************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdlib.h"

#include <errno.h>

#include "callintogtmxfer.h"
#include "gt_timer.h"
#include "have_crit.h"
#include "ydb_logicals.h"

typedef	int	(*int_fptr)();

GBLDEF int (*callintogtm_vectortable[])()=
{
	(int_fptr)hiber_start,
	(int_fptr)hiber_start_wait_any,
	(int_fptr)gtm_start_timer,
	(int_fptr)cancel_timer,
	(int_fptr)gtm_malloc,
	(int_fptr)gtm_free,
	(int_fptr)-1L
};

#ifdef GTM64
#define MAX_ADDR_SIZE                   64
#else
#define MAX_ADDR_SIZE                   32
#endif
#define MAX_ADDR_ENV_SIZE               64
GBLDEF unsigned char    gtmvectortable_address[MAX_ADDR_SIZE];
GBLDEF unsigned char    gtmvectortable_env[2 * MAX_ADDR_ENV_SIZE];	/* 2 to set ydb* and gtm* env vars */

LITREF	char	*ydbenvname[YDBENVINDX_MAX_INDEX];
LITREF	char	*gtmenvname[YDBENVINDX_MAX_INDEX];

error_def(ERR_SYSCALL);

void init_callin_functable(void)
{
	unsigned char 	*env_top, *ptr, *address_top;
	uint4 		address_len;
	int		save_errno, status, i, len;
	char		*envnamestr[2];

	address_top = GTM64_ONLY(i2ascl)NON_GTM64_ONLY(i2asc)(gtmvectortable_address, (UINTPTR_T)(&callintogtm_vectortable[0]));
	*address_top = '\0';
	address_len = (uint4)(address_top - &gtmvectortable_address[0]);
	env_top = &gtmvectortable_env[0];
	/* Set ydb_callin_start and GTM_CALLIN_START env vars to the function pointer array start address */
	envnamestr[0] = (char *)(ydbenvname[YDBENVINDX_CALLIN_START] + 1);	/* + 1 to skip the $ */
	envnamestr[1] = (char *)(gtmenvname[YDBENVINDX_CALLIN_START] + 1);	/* + 1 to skip the $ */
	for (i = 0; i < 2; i++)
	{
		len = strlen(envnamestr[i]);
		ptr = env_top;
		memcpy(ptr, envnamestr[i], len);
		ptr += len;
		*ptr++ = '=';
		memcpy(ptr, gtmvectortable_address, address_len);
		ptr += address_len;
		*ptr++ = '\0';
		assert((ptr - env_top) <= ARRAYSIZE(gtmvectortable_env));
		PUTENV(status, (char *)env_top);
		if (status)
		{
			save_errno = errno;
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(8) ERR_SYSCALL, 5, LEN_AND_LIT("putenv"), CALLFROM, save_errno);
		}
		env_top = ptr;	/* Cannot use same "env_top" for next iteration since PUTENV does not take a copy of pointer
				 * like "setenv" does.
				 */
	}
}
