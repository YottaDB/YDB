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

/* gtm_unistd.h - interlude to <unistd.h> system header file.  */
#ifndef GTM_UNISTDH
#define GTM_UNISTDH

#include <unistd.h>

#define CHDIR		chdir

#define CHOWN		chown

#define GETCWD(buffer,size,getcwd_res)				\
	(getcwd_res = getcwd(buffer, size))

#define GETHOSTNAME(name,namelen,gethostname_res)		\
	(gethostname_res = gethostname(name, namelen))

#define LINK		link

#define UNLINK		unlink

#define TTYNAME		ttyname

#define ACCESS		access

#define EXECL		execl
#define EXECV		execv
#define EXECVE		execve

#define TRUNCATE	truncate

#endif
