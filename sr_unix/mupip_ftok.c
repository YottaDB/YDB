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

#include <errno.h>
#include <unistd.h>
#include <fcntl.h> /* for O_RDONLY */

#include "cli.h"
#include "gtm_stat.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtmio.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "repl_instance.h"
#include "mupip_ftok.h"
#include "mupip_exit.h"

/*
 * This reads file header and prints the semaphore/shared memory id
 * This is different than ftok utility.
 * This is needed because GTM/MUPIP creates semaphore for database access control and
 * shared memory segment using IPC_CREATE flag. GTM saves that ids in database file header.
 * In case of a crash we may need to get those ids. Specially for debugging/testing this is very useful
 * Format is same as ftok output.
 */
void mupip_ftok (void)
{
	boolean_t	jnlpool, recvpool;
	sgmnt_data	header;
	int		semid, shmid;
	unsigned int	full_len;
	unsigned short	fn_len; /* cli library expects unsigned short */
	char		fn[MAX_FN_LEN + 1], instname[MAX_FN_LEN + 1];
	repl_inst_fmt	repl_instance;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_REPLINSTUNDEF);
	error_def(ERR_TEXT);

	fn_len = MAX_FN_LEN;
	if (FALSE == cli_get_str("FILE", fn, &fn_len))
	{
		rts_error(VARLSTCNT(1) ERR_MUPCLIERR);
		mupip_exit(ERR_MUPCLIERR);
	}
	fn[fn_len] = 0;
	jnlpool = (CLI_PRESENT == cli_present("JNLPOOL"));
	recvpool = (CLI_PRESENT == cli_present("RECVPOOL"));
	if (jnlpool || recvpool)
	{
		if (!repl_inst_get_name(instname, &full_len, sizeof(instname)))
			rts_error(VARLSTCNT(6) ERR_REPLINSTUNDEF, 0,
				ERR_TEXT, 2,
				RTS_ERROR_LITERAL("$gtm_repl_instance not defined"));
		repl_inst_get(instname, &repl_instance);
		if (jnlpool)
		{
			semid = repl_instance.jnlpool_semid;
			shmid = repl_instance.jnlpool_shmid;
		} else
		{
			semid = repl_instance.recvpool_semid;
			shmid = repl_instance.recvpool_shmid;
		}
	} else
	{
		if (!file_head_read(fn, &header))
			mupip_exit(ERR_MUNOACTION);
		semid = header.semid;
		shmid = header.shmid;
	}
	PRINTF("Output Format - \n\t%s  ::  %s  ::  %s \n\n", "File", "Semaphore Id", "Share Memory Id");
	PRINTF("Output - \n");
	PRINTF("%20s  ::  %d  [ 0x%x ]  ::  %d  [ 0x%x ] \n", fn, semid, semid, shmid, shmid);
	mupip_exit(SS_NORMAL);
}
