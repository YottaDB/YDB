/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "op.h"
#include "source_file.h"
#include "cli.h"
#include "iosp.h"
#include "mmemory.h"

#ifdef UNIX
GBLREF CLI_ENTRY		*cmd_ary;
GBLREF CLI_ENTRY		mumps_cmd_ary[];
#endif
GBLREF boolean_t		run_time;
GBLREF spdesc			stringpool, rts_stringpool, indr_stringpool;
GBLREF command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF mcalloc_hdr 		*mcavailptr;

#define FILE_NAME_SIZE 255

void op_zcompile(mval *v, boolean_t ignore_dollar_zcompile)
{
	unsigned		status;
	command_qualifier	save_qlf;
	unsigned short		len;
	char			source_file_string[FILE_NAME_SIZE + 1],
				obj_file[FILE_NAME_SIZE + 1],
				list_file[FILE_NAME_SIZE + 1],
				ceprep_file[FILE_NAME_SIZE + 1];
#	ifdef UNIX
	CLI_ENTRY		*save_cmd_ary;
#	endif
	size_t			mcallocated, alloc;
	mcalloc_hdr		*lastmca, *nextmca;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	MV_FORCE_STR(v);
	if (!v->str.len)
		return;
	save_qlf = glb_cmd_qlf;
#	ifdef UNIX	/* VMS retains old behavior, for which $ZCOMPILE only affects ZLINK, not ZCOMPILE or compile mode */
	if (!ignore_dollar_zcompile)
	{
		INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, FILE_NAME_SIZE);
		zl_cmd_qlf(&(TREF(dollar_zcompile)), &cmd_qlf);	/* Process $ZCOMPILE qualifiers */
		glb_cmd_qlf = cmd_qlf;
	}
#	endif
	INIT_CMD_QLF_STRINGS(cmd_qlf, obj_file, list_file, ceprep_file, FILE_NAME_SIZE);
	zl_cmd_qlf(&v->str, &cmd_qlf);			/* Process ZCOMPILE arg. Override any conflicting quals in $ZCOMPILE */
	glb_cmd_qlf = cmd_qlf;
	assert(run_time);
	assert(rts_stringpool.base == stringpool.base);
	rts_stringpool = stringpool;
	if (!indr_stringpool.base)
	{
		stp_init (STP_INITSIZE);
		indr_stringpool = stringpool;
	} else
		stringpool = indr_stringpool;
	run_time = FALSE;
	TREF(compile_time) = TRUE;
	TREF(transform) = FALSE;
	TREF(dollar_zcstatus) = SS_NORMAL;
	len = FILE_NAME_SIZE;
#	ifdef UNIX
	/* The caller of this function could be GT.M, DSE, MUPIP, GTCM GNP server, GTCM OMI server etc. Most of them have their
	 * own command parsing tables and some dont even have one. Nevertheless, we need to parse the string as if it was a
	 * MUMPS compilation command. So we switch temporarily to the MUMPS parsing table "mumps_cmd_ary". Note that the only
	 * rts_errors possible between save and restore of the cmd_ary are in compile_source_file and those are internally
	 * handled by source_ch which will transfer control back to us (right after the the call to compile_source_file below)
	 * and hence proper restoring of cmd_ary is guaranteed even in case of errors.
	 */
	save_cmd_ary = cmd_ary;
	cmd_ary = mumps_cmd_ary;
#	endif
	mcfree();	/* If last compile errored out, may have left things uninitialized for us */
	/* Find out how much space we have in mcalloc blocks */
	for (mcallocated = 0, nextmca = mcavailptr; nextmca; nextmca = nextmca->link)
		mcallocated += nextmca->size;
	if (0 == mcallocated)
		mcallocated = MC_DSBLKSIZE - MCALLOC_HDR_SZ;	/* Min size is one default block size */
	for (status = cli_get_str("INFILE",source_file_string, &len);
		status;
		status = cli_get_str("INFILE",source_file_string, &len))
	{
		compile_source_file(len, source_file_string, FALSE);
		len = FILE_NAME_SIZE;
	}
	/* Determine if need to remove any added added mc blocks. Min value of mcallocated will ensure
	 * we leave at least one block alone.
	 */
	for (alloc = 0, nextmca = mcavailptr;
	     nextmca && (alloc < mcallocated);
	     alloc += nextmca->size, lastmca = nextmca, nextmca = nextmca->link);
	if (nextmca)
	{	/* Start freeing at the nextmca node since these are added blocks */
		lastmca->link = NULL;	/* Sever link to further blocks here */
		/* Release any remaining blocks if any */
		for (lastmca = nextmca; lastmca; lastmca = nextmca)
		{
			nextmca = lastmca->link;
			free(lastmca);
		}
	}
#	ifdef UNIX
	cmd_ary = save_cmd_ary;	/* restore cmd_ary */
#	endif
	assert((FALSE == run_time) && (TRUE == TREF(compile_time)));
	run_time = TRUE;
	TREF(compile_time) = FALSE;
	TREF(transform) = TRUE;
	indr_stringpool = stringpool;
	stringpool = rts_stringpool;
	indr_stringpool.free = indr_stringpool.base;
	glb_cmd_qlf = save_qlf;
}
