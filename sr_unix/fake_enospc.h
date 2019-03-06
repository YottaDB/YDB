/****************************************************************
 *								*
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef FAKE_ENOSPC_H_INCLUDED
#define FAKE_ENOSPC_H_INCLUDED

#define ENOSPC_INIT_DURATION	(8 * MILLISECS_IN_SEC)

void fake_enospc(void);
void handle_deferred_syslog(void);
#endif
