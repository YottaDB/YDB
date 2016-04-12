/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_unistd.h"
#include "gtm_fcntl.h" /* for O_RDONLY */
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "cli.h"
#include "gtm_stat.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gtmio.h"
#include "iosp.h"
#include "eintr_wrappers.h"
#include "repl_instance.h"
#include "mupip_ftok.h"
#include "mupip_exit.h"
#include "file_head_read.h"	/* for file_head_read() prototype */
#include "is_file_identical.h"	/* for filename_to_id() prototype */

GBLREF	boolean_t		in_mupip_ftok;		/* Used by an assert in repl_inst_read */

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
	char		fn[MAX_FN_LEN + 1], instfilename[MAX_FN_LEN + 1];
	repl_inst_hdr	repl_instance;
	gd_id		fid;
	sm_uc_ptr_t	fid_ptr, fid_top;

	error_def(ERR_MUPCLIERR);
	error_def(ERR_MUNOACTION);
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
		if (!repl_inst_get_name(instfilename, &full_len, SIZEOF(instfilename), issue_rts_error))
			GTMASSERT;	/* rts_error should have been issued by repl_inst_get_name */
		in_mupip_ftok = TRUE;
		repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
		in_mupip_ftok = FALSE;
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
		if (!file_head_read(fn, &header, SIZEOF(header)))
			mupip_exit(ERR_MUNOACTION);
		semid = header.semid;
		shmid = header.shmid;
	}
	PRINTF("%20s  ::  %23s  ::  %23s  ::  %20s\n", "File", "Semaphore Id", "Shared Memory Id", "FileId");
	PRINTF("---------------------------------------------------------------------------------------------------------------\n");
	PRINTF("%20s  ::  %10d [0x%.8x]  ::  %10d [0x%.8x]  ::  0x", fn, semid, semid, shmid, shmid);
	fid_ptr = (sm_uc_ptr_t)&fid;
	filename_to_id((gd_id_ptr_t)fid_ptr, fn);
	fid_top = fid_ptr + SIZEOF(fid);
	for ( ; fid_ptr < fid_top; fid_ptr++)
		PRINTF("%.2x", *(sm_uc_ptr_t)fid_ptr);
	PRINTF("\n");
	mupip_exit(SS_NORMAL);
}
