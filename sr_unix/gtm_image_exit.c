/****************************************************************
 *								*
 * Copyright (c) 2015 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_stdlib.h"

#include "gtm_multi_thread.h"	/* needed for below EXIT macro */

void	gtm_image_exit(int status)
{						\
	char	*rname;				\
						\
	if (!INSIDE_THREADED_CODE(rname))	\
		exit(status);			\
	else					\
		GTM_PTHREAD_EXIT(status);	\
}
