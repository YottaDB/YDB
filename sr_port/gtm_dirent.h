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

/* gtm_dirent.h - interlude to <dirent.h> system header file.  */
#ifndef GTM_DIRENTH
#define GTM_DIRENTH

#include <dirent.h>

#define OPENDIR		opendir

#define READDIR(dir, rddr_res)	(rddr_res = readdir(dir))

#endif
