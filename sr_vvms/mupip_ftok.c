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
#include <ssdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "vmsdtype.h"
#include "gtm_logicals.h"
#include "cli.h"
#include "util.h"
#include "is_file_identical.h"
#include "mupip_exit.h"

#define MAX_PATH_LEN		256
#define MAX_FTOK_LEN		32		/* 1 for length, 31 for the actual ftok */
#define FTOK_PREFIX_SIZE	4

void mupip_ftok()
{
	unsigned short	prefix_size;
	unsigned short	filename_len;
	char		filename[MAX_PATH_LEN], prefix[FTOK_PREFIX_SIZE] = {'G', 'T', '$', 'S'};
	gds_file_id	gds_fid;
	char		ftok[MAX_FTOK_LEN];
	error_def(ERR_MUPCLIERR);

	filename_len	= SIZEOF(filename);
	prefix_size	= SIZEOF(prefix);
	if (!cli_get_str("FILE", filename, &filename_len))
		mupip_exit(ERR_MUPCLIERR);
	if (CLI_PRESENT == cli_present("PREFIX") && !cli_get_str("PREFIX", prefix, &prefix_size))
		mupip_exit(ERR_MUPCLIERR);
	set_gdid_from_file(&gds_fid, filename, filename_len);
	global_name(prefix, &gds_fid, ftok);
	util_out_print("!AD", TRUE, (char) ftok[0], &ftok[1]);
	mupip_exit(SS$_NORMAL);
}
