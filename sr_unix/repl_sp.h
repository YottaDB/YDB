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

#ifndef REPL_SP_H_INCLUDED
#define REPL_SP_H_INCLUDED

#define ERRNO           	(errno)
#define FORMAT_STR      	"PID %d %s is%s alive\n"
#define FORMAT_STR1      	"PID %d %s is%s alive in %s mode\n"

/*----- File I/O related -----*/
#define F_CLOSE(FD, RC)				CLOSEFILE_RESET(FD, RC) /* interrupt safe close(); also resets "FD" to FD_INVALID */
#define F_COPY_GDID(to, from)			memcpy(&(to), &(from), SIZEOF(to))
#define F_COPY_GDID_FROM_STAT(to, stat_buf) 	set_gdid_from_stat(&(to), &stat_buf);
#define F_READ_BLK_ALIGNED			LSEEKREAD
#endif
