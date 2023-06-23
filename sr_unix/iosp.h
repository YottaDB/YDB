/****************************************************************
 *								*
 *	Copyright 2001, 2008 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "gtm_ctype.h"

#define SS_NORMAL  0
#define SS_NOLOGNAM -1
#define SS_LOG2LONG -2
#define SS_ENDOFTAPE 2
#define SS_ENDOFFILE 4

/*
 * To avoid any possible deadlock because of ftok collisions, we use different project ids.
 */
#define GTM_ID		43
#define REPLPOOL_ID	44
#define GTMSECSHR_ID	46
#define RALL 		0444
#define RWALL 		0666
#define RWDALL 		0777

/* parameters for io_rundown() */
#define NORMAL_RUNDOWN		0
#define RUNDOWN_EXCEPT_STD	1

#define SYSCALL_SUCCESS(STATUS)		(SS_NORMAL == (STATUS))
#define SYSCALL_ERROR(STATUS)		(SS_NORMAL != (STATUS))
