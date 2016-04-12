/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef __GTM_STARTUP_CHK_H__
#define __GTM_STARTUP_CHK_H__

#if defined(__linux__) || defined(__CYGWIN__)
#define PROCSELF	"/proc/self/exe"
#elif defined(__sparc)
#define PROCSELF	"/proc/%d/path/a.out"
#elif defined(_AIX)
#define PROCSELF	"/proc/%d/object/a.out"
#endif

int gtm_chk_dist(char *image);
int gtm_image_path(char *realpath);
#endif
