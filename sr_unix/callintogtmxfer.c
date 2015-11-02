/****************************************************************
 *								*
 *	Copyright 2001, 2007 Fidelity Information Services, Inc	*
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

typedef	int	(*int_fptr)();
GBLREF  int     jnlpool_detach();

GBLDEF int (*callintogtm_vectortable[])()=
{
	(int_fptr)hiber_start,
	(int_fptr)hiber_start_wait_any,
	(int_fptr)start_timer,
	(int_fptr)cancel_timer,
	(int_fptr)malloc,
	(int_fptr)free,
	(int_fptr)jnlpool_detach,
	(int_fptr)-1L
};

#ifdef GTM64
#define MAX_ADDR_SIZE                   64
#else
#define MAX_ADDR_SIZE                   32
#endif
#define MAX_ADDR_ENV_SIZE               64
#define GTM_CALLIN_START_ENV            "GTM_CALLIN_START="
GBLDEF unsigned char    gtmvectortable_address[MAX_ADDR_SIZE];
GBLDEF unsigned char    gtmvectortable_env[MAX_ADDR_ENV_SIZE];

void init_callin_functable(void)
{
	unsigned char 	*env_top, *address_top;
	uint4 		address_len;

	address_top = GTM64_ONLY(i2ascl)NON_GTM64_ONLY(i2asc)(gtmvectortable_address, (UINTPTR_T)(&callintogtm_vectortable[0]));
	*address_top = '\0';
	address_len = (uint4)(address_top - &gtmvectortable_address[0]);
	env_top = &gtmvectortable_env[0];
	memcpy(env_top, GTM_CALLIN_START_ENV, strlen(GTM_CALLIN_START_ENV));
	memcpy((env_top + strlen(GTM_CALLIN_START_ENV)), gtmvectortable_address, address_len);
	*(env_top + strlen(GTM_CALLIN_START_ENV) + address_len) = '\0';
	if (PUTENV((char *)gtmvectortable_env))
	{
		rts_error(VARLSTCNT(1) errno);
	}
}
