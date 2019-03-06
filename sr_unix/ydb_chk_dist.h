/****************************************************************
 *								*
<<<<<<< HEAD:sr_unix/ydb_chk_dist.h
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
=======
 * Copyright (c) 2001-2018 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 7a1d2b3e... GT.M V6.3-007:sr_unix/gtm_startup_chk.h
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GTM_STARTUP_CHK_H_INCLUDED
#define GTM_STARTUP_CHK_H_INCLUDED

#if defined(__linux__)
#define PROCSELF	"/proc/self/exe"
#endif

int ydb_chk_dist(char *image);

#endif
