/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "mupipbckup.h"
#include "gdscc.h"
#include "gdskill.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "util.h"
#include "cli.h"
#include "mupip_exit.h"
#include "str_match.h"
#include "mu_getlst.h"

GBLDEF	mstr		directory;
GBLDEF	bool 		is_directory;
GBLREF 	bool		error_mupip;
GBLREF 	bool		in_backup;
GBLREF	gd_addr 	*gd_header;
GBLREF	tp_region	*grlist;
GBLREF	boolean_t	mu_star_specified;

void mu_getlst(char *name, int4 size)
{
	char		*c1, *c2, *c3, *c4, rbuff[MAX_FN_LEN + 1], fbuff[MAX_FN_LEN + 1];
	unsigned short	rlen, flen, i;
	gd_region	*reg;
	tp_region	*list;
	boolean_t	matched;

	error_def(ERR_MUNODBNAME);
	error_def(ERR_MUBCKNODIR);
	error_def(ERR_MUNOACTION);
	error_def(ERR_TEXT);

	mu_star_specified = FALSE;
	assert(size > 0);
	rlen = sizeof(rbuff);
	flen = sizeof(fbuff);
	if (!cli_get_str(name, rbuff, &rlen))
		mupip_exit(ERR_MUNODBNAME);
	if (in_backup && ((!cli_get_str("SAVE_DIR", fbuff, &flen)) || (0 == flen)))
		mupip_exit(ERR_MUBCKNODIR);

	is_directory = FALSE;
	for (c1 = c2 = rbuff, c3 = c4 = fbuff;;)
	{
		for (; *c2 && (*c2 != ','); c2++) /* locate a reg spec */
			;
		if (c2 - c1 > MAX_RN_LEN)
		{
			error_mupip = TRUE;
			util_out_print("!UL exceeds maximum REGION name length of !UL characters.", TRUE, c2 - c1, MAX_RN_LEN);
		} else
		{	/* handle the reg spec here */
			if ('*' == *c1 && (1 == c2 - c1))
				mu_star_specified = TRUE;
			matched = FALSE;
			for (i = 0, reg = gd_header->regions; i < gd_header->n_regions; i++, reg++)
			{
				if (TRUE == str_match((char *)reg->rname, reg->rname_len, c1, c2 - c1))
				{
					matched = TRUE;
					if (NULL == (list = insert_region(reg, &(grlist), NULL, size)))
					{
						error_mupip = TRUE;
						rts_error(VARLSTCNT(4) ERR_TEXT, 2, RTS_ERROR_STRING("Region not found"));
						continue;
					}
					if ((FALSE == in_backup) || (0 != ((backup_reg_list *)list)->backup_file.len))
						continue;
					if (TRUE == is_directory)
					{
						assert(NULL != grlist->fPtr);
						mubexpfilnam(directory.addr, directory.len, (backup_reg_list *)list);
					} else
					{
						for (; *c4 && (*c4 != ',');  c4++) /* locate a file spec */
							;
						if (FALSE == mubgetfil((backup_reg_list *)list, c3, c4 - c3))
							break;
						if (*c4)
							c3 = ++c4;
						else if (FALSE == is_directory)
							break;
					}
				}
			}
			if (!matched)
			{
				util_out_print("REGION !AD not found", TRUE, c2 - c1, c1);
				mupip_exit(ERR_MUNOACTION);
			}
		}
		if (!*c2)
			break;
		else
			c1 = ++c2;
	}
	return;
}
