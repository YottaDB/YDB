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

#include <errno.h>

#include "gtm_stdlib.h"
#include "callintogtmxfer.h"

typedef	int	(*int_fptr)();

int	hiber_start();
int	hiber_start_wait_any();
int	start_timer();
int	cancel_timer();
GBLREF	int	jnlpool_detach();

GBLDEF int (*callintogtm_vectortable[])()=
{
	hiber_start,
	hiber_start_wait_any,
	start_timer,
	cancel_timer,
	(int_fptr)gtm_malloc,
	(int_fptr)gtm_free,
	jnlpool_detach,
	(int_fptr)(-1)
};

#define MAX_ADDR_SIZE                   32
#define MAX_ADDR_ENV_SIZE               64
#define GTM_CALLIN_START_ENV            "GTM_CALLIN_START="
GBLDEF unsigned char    gtmvectortable_address[MAX_ADDR_SIZE];
GBLDEF unsigned char    gtmvectortable_env[MAX_ADDR_ENV_SIZE];

void init_callin_functable(void)
{
	unsigned char 	*env_top, *address_top;
	uint4		address_len;

	/* The address of the vector table containing pointers
	 * to gt_timers functions is of type unsigned int which
	 * is o.k in current GT.M implementations, however when
	 * GT.M migrates to a fully 64 port this part of the code
	 * might have be re-visited.
	 */
	assert ( 64 > sizeof(gtmvectortable_address));
	address_top = i2asc(gtmvectortable_address, (uint4 )&callintogtm_vectortable[0]);
	*address_top = '\0';
	address_len = (uint4 )(address_top - &gtmvectortable_address[0]);
	env_top =  &gtmvectortable_env[0];
	memcpy(env_top, GTM_CALLIN_START_ENV, strlen(GTM_CALLIN_START_ENV));
	memcpy((env_top + strlen(GTM_CALLIN_START_ENV)), gtmvectortable_address, address_len);
	*(env_top + strlen(GTM_CALLIN_START_ENV) + address_len) = '\0';
	if (PUTENV((char *)gtmvectortable_env))
	{
		rts_error(VARLSTCNT(1) errno);
	}
}
