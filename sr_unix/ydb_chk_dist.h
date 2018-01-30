/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GTM_STARTUP_CHK_H__
#define __GTM_STARTUP_CHK_H__

#if defined(__linux__)
#define PROCSELF	"/proc/self/exe"
#endif

int ydb_chk_dist(char *image);

#endif
