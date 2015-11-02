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

#include "gtm_string.h"

#include "stp_parms.h"
#include "stringpool.h"
#include "cmd_qlf.h"
#include "cgp.h"
#include "compiler.h"
#include "mmemory.h"

GBLREF boolean_t		run_time;
GBLREF bool			compile_time;
GBLREF spdesc			stringpool, rts_stringpool, indr_stringpool;
GBLREF char			source_file_name[];
GBLREF unsigned short		source_name_len;
GBLREF command_qualifier	cmd_qlf, glb_cmd_qlf;
GBLREF char			cg_phase;
GBLREF int4			dollar_zcstatus;
GBLREF bool			transform;
GBLREF mcalloc_hdr 		*mcavailptr, *mcavailbase;

int zlcompile (unsigned char len, unsigned char *addr)
{
	boolean_t	obj_exp, status;
	size_t		mcallocated, alloc;
	mcalloc_hdr	*lastmca, *nextmca;

	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLNOOBJECT);

	memcpy (source_file_name, addr, len);
	source_file_name[len] = 0;
	source_name_len = len;

	assert(run_time);
	obj_exp = (cmd_qlf.qlf & CQ_OBJECT) != 0;
	assert(rts_stringpool.base == stringpool.base);
	rts_stringpool = stringpool;
	if (!indr_stringpool.base)
	{
		stp_init (STP_INITSIZE);
		indr_stringpool = stringpool;
	} else
		stringpool = indr_stringpool;

	run_time = FALSE;
	compile_time = TRUE;
	transform = FALSE;
	/* Find out how much space we have in mcalloc blocks */
	assert(mcavailptr == mcavailbase);
	for (mcallocated = 0, nextmca = mcavailptr; nextmca; nextmca = nextmca->link)
		mcallocated += nextmca->size;
	if (0 == mcallocated)
		mcallocated = MC_DSBLKSIZE;	/* Min size is one default block size */
	status = compiler_startup();
	/* Determine if need to remove any added added mc blocks. Min value of mcallocated will ensure
	   we leave at least one block alone.
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
	assert (run_time == FALSE && compile_time == TRUE);
	run_time = TRUE;
	compile_time = FALSE;
	transform = TRUE;
	indr_stringpool = stringpool;
	stringpool = rts_stringpool;
	indr_stringpool.free = indr_stringpool.base;
	cmd_qlf.qlf = glb_cmd_qlf.qlf;
	if (obj_exp && cg_phase != CGP_FINI)
		rts_error(VARLSTCNT(5) ERR_ZLINKFILE, 2, len, addr, ERR_ZLNOOBJECT);
	return status;
}
