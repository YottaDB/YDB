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

#define DAEMON_PATH "$gtm_dist/gtm_dmna"
#define DAEMON_FILE_NUM 200

#define DAEMON_OPEN 1
#define DAEMON_WRITE 2
#define DAEMON_CLOSE 3
#define DAEMON_JOURNAL 4

#ifndef NBPG
#define NBPG 4096
#endif
