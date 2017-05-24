/****************************************************************
 *								*
 * Copyright (c) 2016 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtmxc_types.h"

#ifdef GTM_PTHREAD
#  include "gtm_pthread.h"
#endif

#ifdef GTM_PTHREAD
GBLREF	pthread_t		gtm_main_thread_id;
GBLREF	boolean_t		gtm_main_thread_id_set;
#endif

int gtm_is_main_thread()
{
# 	ifdef GTM_PTHREAD
	if (!gtm_main_thread_id_set)
		return -1;
	if (pthread_equal(gtm_main_thread_id, pthread_self()))
		return 1;
	return 0;
#	else
	return -1;
#	endif
}


