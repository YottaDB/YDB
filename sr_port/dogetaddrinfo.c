/****************************************************************
 *								*
 * Copyright (c) 2019 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_socket.h"
#include "gtm_netdb.h"

#include "dogetaddrinfo.h"
#include "have_crit.h"

/* Routine to do a getaddrinfo() call surrounding it with DEFER/ENABLE_INTERRUPT macros
 * so it cannot nest. Nesting has been shown to create deadlocks so must be avoided.
 * This is a wrapper instead of a macro because the normal usage of getaddrinfo() is in
 * an assignment statement within a conditional.
 */
int dogetaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
	int	status, prev_intrpt_state;

	DEFER_INTERRUPTS(INTPRT_IN_DO_GETADDRINFO, prev_intrpt_state);
	status = getaddrinfo(node, service, hints, res);
	ENABLE_INTERRUPTS(INTPRT_IN_DO_GETADDRINFO, prev_intrpt_state);
	return status;
}
