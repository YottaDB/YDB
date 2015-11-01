/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GTM_TEMPNAM_H__
#define __GTM_TEMPNAM_H__

void gtm_tempnam(char *dir, char *prefix, char *fullname);

#if defined(UNIX)
# define SCRATCH_DIR "/tmp/"
#elif defined(VMS)
# define SCRATCH_DIR "SYS$SCRATCH:"
#else
# error Unsupported Platform
#endif
#endif
