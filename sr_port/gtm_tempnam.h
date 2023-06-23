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

#ifndef GTM_TEMPNAM_H_INCLUDED
#define GTM_TEMPNAM_H_INCLUDED

void gtm_tempnam(char *dir, char *prefix, char *fullname);

#if defined(UNIX)
# define SCRATCH_DIR "/tmp/"
#elif defined(VMS)
# define SCRATCH_DIR "SYS$SCRATCH:"
#else
# error Unsupported Platform
#endif
#endif
