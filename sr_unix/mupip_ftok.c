/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_ipc.h"
#include "gtm_unistd.h"
#include "gtm_fcntl.h" /* for O_RDONLY */
#include "gtm_stdio.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_sizeof.h"
#include "cli.h"
#include "parse_file.h"
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
#include "is_gdid.h"	/* for filename_to_id() prototype */

GBLREF	boolean_t		in_mupip_ftok;		/* Used by an assert in repl_inst_read */

error_def(ERR_MUPCLIERR);
error_def(ERR_MUNOACTION);
error_def(ERR_TEXT);

/*
 * By default. this reads file header and prints the semaphore/shared memory id
 * This is needed because GTM/MUPIP creates semaphore for database access control and
 * shared memory segment using IPC_CREATE flag. GTM saves that ids in database file header.
 * In case of a crash we may need to get those ids. Specially for debugging/testing this is very useful
 * Format is filename ftok::ftok for the -only case, bases 10 & 16 respectively,
 * and described by the optional headers below for the others.
 */
void mupip_ftok(void)
{
	boolean_t	fd, jnlpool, only, recvpool, showheader;
	char		fn[MAX_FN_LEN + 1], instfilename[MAX_FN_LEN + 1], replf[MAX_FN_LEN + 1];
	gd_id		fid;
	int		index, ispool, semid, shmid;
	int4            id, status;
	key_t           semkey;
	mstr		file;
	parse_blk	pblk;
	repl_inst_hdr	repl_instance;
	sgmnt_data	header;
	sm_uc_ptr_t	fid_ptr, fid_top;
	unsigned int	full_len;
	unsigned short	fn_len; /* cli library expects unsigned short */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (FALSE == cli_get_int("ID", &id))
		id = GTM_ID;
	only = (CLI_PRESENT == cli_present("ONLY"));
	jnlpool = (CLI_PRESENT == cli_present("JNLPOOL"));
	recvpool = (CLI_PRESENT == cli_present("RECVPOOL"));
	ispool = jnlpool + recvpool;
	if (ispool)						/* forget any parameters and use parms_cnt as loop control*/
		TREF(parms_cnt) = ispool + 1;
	showheader = (!cli_negated("HEADER"));
	index = (only && ispool);				/* if only, skip the instance file  */
	for (; index < TREF(parms_cnt); index++)
	{	/*  in order to handle multiple files, this loop directly uses the array built by cli */
		if (ispool)
		{	/* ispool idicates we're precessing based on the replication instance file */
			if (only == index)
			{	/* first pass - process the instance file */
				if (!repl_inst_get_name(instfilename, &full_len, SIZEOF(instfilename), issue_rts_error, NULL))
					assertpro(NULL == instfilename);	/* otherwise, repl_inst_get_name issues rts_error */
				in_mupip_ftok = TRUE;	/* this flag implicitly relies on mupip ftok being once and done */
				repl_inst_read(instfilename, (off_t)0, (sm_uc_ptr_t)&repl_instance, SIZEOF(repl_inst_hdr));
				in_mupip_ftok = FALSE;	/* no condition hander to reset this in case of error - see comment above */
				memset(&pblk, 0, SIZEOF(pblk));
				pblk.buff_size = MAX_FN_LEN;
				pblk.buffer = replf;
				pblk.fop = F_SYNTAXO;
				file.addr = instfilename;
				file.len = full_len;
				status = parse_file(&file, &pblk);	/* this gets us to directory, name and extension */
				fn_len = pblk.b_dir + pblk.b_name + pblk.b_ext;
				memcpy(fn, pblk.l_dir, fn_len);
				fn[fn_len] = 0;
				semkey = FTOK(fn, REPLPOOL_ID);
				semid = shmid = -1;			/* not relevant for the file */
			}
			if (jnlpool && (1 == index))
			{	/* goes first if also -recvpool */
				semid = repl_instance.jnlpool_semid;
				shmid = repl_instance.jnlpool_shmid;
				fn_len = SIZEOF("jnlpool");
				strncpy(fn, "jnlpool", fn_len);
			} else if (recvpool && ((jnlpool ? 2 : 1) == index))
			{	/* last or sole */
				semid = repl_instance.recvpool_semid;
				shmid = repl_instance.recvpool_shmid;
				fn_len = SIZEOF("recvpool");
				strncpy(fn, "recvpool", fn_len);
			}
		} else
		{	/* not instance file based */
			strncpy(fn, TAREF1(parm_ary, index), MAX_FN_LEN);
			semkey = FTOK(fn, id);
			assert(semkey);
			if (!only)
			{	/* try as a database file */
				OPENFILE(fn, O_RDONLY, fd); /* if OPEN works, file_head_read takes care of close if other issues */
				if ((FD_INVALID == fd) || (!file_head_read(fn, &header, SIZEOF(header))))
				{
					FPRINTF(stderr, "%s is not a database file\n",fn);
					FPRINTF(stderr, "This and any subsequent files are treated as -only\n");
					only = TRUE;
				} else
				{
					semid = header.semid;
					shmid = header.shmid;
				}
			}
		}
		if (!only || ispool)
		{
			if (showheader)
			{
				FPRINTF(stderr, "%20s :: %23s :: %23s :: %23s :: %34s\n", "File", "Semaphore Id",
					"Shared Memory Id", "FTOK Key", "FileId");
				FPRINTF(stderr, "-----------------------------------------------------------------------------");
				FPRINTF(stderr, "--------------------------------------------------------------\n");
				showheader = FALSE;
			}
			if (!ispool || !index)
			{	/* it's a file */
				FPRINTF(stderr, "%20s :: %10d [0x%.8x] :: %10d [0x%.8x] :: %10d [0x%.8x] :: 0x", fn, semid, semid,
					shmid, shmid, semkey, semkey);
				fid_ptr = (sm_uc_ptr_t)&fid;
				filename_to_id((gd_id_ptr_t)fid_ptr, fn);
				for (fid_top = fid_ptr + SIZEOF(fid) ; fid_ptr < fid_top; fid_ptr++)
					FPRINTF(stderr, "%.2x", *(sm_uc_ptr_t)fid_ptr);
			} else
				FPRINTF(stderr, "%20s :: %10d [0x%.8x] :: %10d [0x%.8x]", fn, semid, semid, shmid, shmid);
			FPRINTF(stderr, "\n");
		} else		/* simple legacy format */
			FPRINTF(stderr, "%20s  ::  %10d  [ 0x%8x ]\n", fn, semkey, semkey);
	}
	mupip_exit(SS_NORMAL);
}
