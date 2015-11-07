/****************************************************************
 *								*
 *	Copyright 2013, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <sys/types.h>
#include "gtm_stat.h"
#include "gtm_stdlib.h"
#include "gtm_string.h"
#include "gtm_limits.h"

#include "cmd_qlf.h"
#include "gtmio.h"
#include "parse_file.h"

GBLDEF char			tmp_object_file_name[MAX_FBUFF + 1];

GBLREF command_qualifier	cmd_qlf;
GBLREF char			object_file_name[];
GBLREF short			object_name_len;
GBLREF mident			module_name;

error_def(ERR_FILEPARSE);
error_def(ERR_OBJFILERR);

/* Open object file - TODO: make into macro instead of routine */
int open_object_file(const char *fname, int fflag)
{
	int		fdesc;

	OPENFILE3(fname, fflag, 0666, fdesc);
	return fdesc;
}

/* TODO - Move these routines to obj_file.c */
/* Routine to create a temporary object file. This file is created in the directory it is supposed to reside in but is created
 * with a temporary name. When complete, it is renamed to what it was meant to be replacing the previous version.
 *
 * Parameters:
 *
 *   object_fname     - Character array of the object path/name.
 *   object_fname_len - Length of that array in bytes.
 *
 * Output:
 *   File descriptor for the open object file.
 */
int mk_tmp_object_file(const char *object_fname, int object_fname_len)
{
	int	fdesc, status;
	int	umask_creat, umask_orig;

	/* TODO Verify addition of XXXXXX doesn't exceed dimensions of array */
	memcpy(tmp_object_file_name, object_fname, object_fname_len);
        memcpy(&tmp_object_file_name[object_fname_len], "XXXXXX", 7);	/* Includes null terminator */
	fdesc = mkstemp(tmp_object_file_name);
	if (FD_INVALID == fdesc)
	{
		printf("tmp_object_file_name: %s\n", tmp_object_file_name);
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_OBJFILERR, 2, object_fname_len, object_fname, errno);
	}
	umask_orig = umask(000);	/* Determine umask (destructive) */
	(void)umask(umask_orig);	/* Reset umask */
	umask_creat = 0666 & ~umask_orig;
	/* TODO: set protections appropriately (if this isn't it) */
	FCHMOD(fdesc, umask_creat);
        return fdesc;
}

void rename_tmp_object_file(const char *object_fname)
{
	rename(tmp_object_file_name, object_fname);	/* TODO - Handle error (SEE) and make into macro instead of routine */
}

/*
 * Inputs: cmd_qlf.object_file, module_name
 * Outputs: object_file_name, object_name_len
 */
void init_object_file_name(void)
{
	int		status, rout_len;
	char		obj_name[SIZEOF(mident_fixed) + 5];
	mstr		fstr;
	parse_blk	pblk;

	memset(&pblk, 0, SIZEOF(pblk));
	pblk.buffer = object_file_name;
	pblk.buff_size = MAX_FBUFF;

	/* Create the object file */
	fstr.len = (MV_DEFINED(&cmd_qlf.object_file) ? cmd_qlf.object_file.str.len : 0);
	fstr.addr = cmd_qlf.object_file.str.addr;
	rout_len = (int)module_name.len;
	memcpy(&obj_name[0], module_name.addr, rout_len);
	memcpy(&obj_name[rout_len], DOTOBJ, SIZEOF(DOTOBJ));    /* includes null terminator */
	pblk.def1_size = rout_len + SIZEOF(DOTOBJ) - 1;         /* Length does not include null terminator */
	pblk.def1_buf = obj_name;
	status = parse_file(&fstr, &pblk);
	if (0 == (status & 1))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, fstr.len, fstr.addr, status);

	object_name_len = pblk.b_esl;
	object_file_name[object_name_len] = '\0';
}
