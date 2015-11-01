/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc.*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_SP_H
#define _REPL_SP_H

#define ERRNO           	(errno)
#define FORMAT_STR      	"PID %d %s is%s alive\n"
#define FORMAT_STR1      	"PID %d %s is%s alive in %s mode\n"
#define repl_fork_rcvr_server(pid, d1) (((*(pid) = fork()) >= 0) ? SS_NORMAL : *(pid))

/*----- File I/O related -----*/
#define F_CLOSE(FD, RC)				CLOSEFILE(FD, RC) /* interrupt safe close() */
#define F_COPY_GDID(to, from)			memcpy(&(to), &(from), sizeof(to))
#define F_COPY_GDID_FROM_STAT(to, stat_buf) 	set_gdid_from_stat(&(to), &stat_buf);
#define F_READ_BLK_ALIGNED			LSEEKREAD
#define F_WRITE_BLK_ALIGNED			LSEEKWRITE
#endif
