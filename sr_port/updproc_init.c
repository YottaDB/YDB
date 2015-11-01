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

#include "mdef.h"

#if !defined(__MVS__) && !defined(VMS)
#include <sys/param.h>
#endif
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#ifdef UNIX
#include <sys/sem.h>
#endif
#ifdef VMS
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmrecv.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "jnl.h"
#include "muprec.h"
#include "repl_log.h"
#include "repl_sem.h"
#include "mupip_exit.h"
#include "dpgbldir.h"
#include "updproc.h"

GBLDEF  upd_proc_ctl    *upd_db_files;

GBLREF	uint4		process_id;
GBLREF	recvpool_addrs	recvpool;
GBLREF  gd_addr         *gd_header;
GBLREF  boolean_t        repl_enabled;
GBLREF  boolean_t        pool_init;

error_def(ERR_UPDATEFILEOPEN);

int updproc_init(void)
{
	mval            v;
	uint4		log_file_len;

	error_def(ERR_RECVPOOLSETUP);
	error_def(ERR_TEXT);

	recvpool_init(UPDPROC, FALSE, FALSE);
	updproc_log_init();

	/*
	 * Lock the update process count semaphore. Its value should
	 * be atmost 1
	 */

	if (0 != grab_sem_immediate(RECV, UPD_PROC_COUNT_SEM))
	{
		if (REPL_SEM_NOT_GRABBED)
			return(UPDPROC_EXISTS);
		else
			rts_error(VARLSTCNT(7) ERR_RECVPOOLSETUP, 0, ERR_TEXT, 2,
				RTS_ERROR_LITERAL("Receive pool semop error"), REPL_SEM_ERRNO);
	}

	v.mvtype = MV_STR; /* get the desired global directory */
        v.str.len = 0;
        gd_header = zgbldir(&v);

	upd_db_files = read_db_files_from_gld(gd_header);/* read the global directory read
					all the database files to be opened */

        if (!upd_open_files(&upd_db_files)) /* open and initialize all regions */
                mupip_exit(ERR_UPDATEFILEOPEN);
	if (repl_enabled)
		jnlpool_init((jnlpool_user)GTMPROC, (boolean_t)FALSE, (boolean_t *)NULL);
	return(UPDPROC_STARTED);
}
