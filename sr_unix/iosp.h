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

#include "gtm_ctype.h"

#define SS_NORMAL  0
#define SS_NOLOGNAM  ((uint4)-1)
#define SS_ENDOFTAPE 2
#define SS_ENDOFFILE 4

/*
 * To avoid any possible deadlock because of ftok collisions, we use different project ids.
 */
#define GTM_ID		43
#define JNLPOOL_ID	44
#define RECVPOOL_ID	45
#define GTMSECSHR_ID	46
#define RWALL 0666
#define RWDALL 0777

/* parameters for io_rundown() */
#define NORMAL_RUNDOWN		0
#define RUNDOWN_EXCEPT_STD	1
