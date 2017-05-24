/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "sys/types.h"
#include "gtm_stat.h"
#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_limits.h"

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
#include "gtmmsg.h"
#include "gtm_reservedDB.h"

GBLDEF	boolean_t		is_directory;
GBLDEF	mstr			directory;

GBLREF	backup_reg_list		*mu_repl_inst_reg_list;
GBLREF 	bool			error_mupip;
GBLREF 	bool			in_backup;
GBLREF	boolean_t		mu_star_specified;
GBLREF	gd_addr			*gd_header;
GBLREF	tp_region		*grlist;

error_def(ERR_MUBCKNODIR);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNODBNAME);
error_def(ERR_MUPCLIERR);
error_def(ERR_NOREGION);

#define	CHECK_IF_NOT_ABSENT(QUALIFIER)										\
{														\
	if (CLI_ABSENT != cli_present(QUALIFIER))								\
	{													\
		util_out_print(QUALIFIER " cannot be specified without specifying a backup region", TRUE);	\
		mupip_exit(ERR_MUPCLIERR);									\
	}													\
}

void mu_getlst(char *name, int4 size)
{
	boolean_t	matched, is_statsDB, is_autoDB;
	char		*c1, *c2, *c3, *c4, fbuff[GTM_PATH_MAX], rbuff[GTM_PATH_MAX], fnbuff[GTM_PATH_MAX + 1];
	gd_region	*reg;
	tp_region	*list;
	unsigned short	flen, i, rlen;
	struct stat	stat_buf;
	int		rc;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	mu_star_specified = FALSE;
	assert(size > 0);
	rlen = SIZEOF(rbuff);
	flen = SIZEOF(fbuff);
	is_directory = FALSE;
	if (!in_backup)
	{
		if (!cli_get_str(name, rbuff, &rlen))
			mupip_exit(ERR_MUNODBNAME);
		for (i = 0; i < rlen; i++)
			rbuff[i] = TOUPPER(rbuff[i]); /* Region names are always upper-case ASCII and thoroughly NUL terminated */
		for ( ; i < ARRAYSIZE(rbuff); i++)
			rbuff[i] = 0;
	} else
	{
		if (CLI_PRESENT == cli_present("REPLINSTANCE"))
		{	/* Region corresponding to the replication instance file has been specified. */
			if (0 == TREF(parms_cnt))
			{	/* No REG_NAME or SAVE_DIR parameter specified in command line. Disable other backup qualifiers. */
				CHECK_IF_NOT_ABSENT("BKUPDBJNL");
				CHECK_IF_NOT_ABSENT("BYTESTREAM");
				CHECK_IF_NOT_ABSENT("COMPREHENSIVE");
				CHECK_IF_NOT_ABSENT("DATABASE");
				CHECK_IF_NOT_ABSENT("DBG");
				CHECK_IF_NOT_ABSENT("INCREMENTAL");
				CHECK_IF_NOT_ABSENT("JOURNAL");
				CHECK_IF_NOT_ABSENT("NETTIMEOUT");
				CHECK_IF_NOT_ABSENT("NEWJNLFILES");
				CHECK_IF_NOT_ABSENT("ONLINE");
				CHECK_IF_NOT_ABSENT("RECORD");
				CHECK_IF_NOT_ABSENT("REPLICATION");
				CHECK_IF_NOT_ABSENT("SINCE");
				CHECK_IF_NOT_ABSENT("TRANSACTION");
			}
			assert(NULL == mu_repl_inst_reg_list);
			if (NULL == mu_repl_inst_reg_list)
				mu_repl_inst_reg_list = malloc(SIZEOF(backup_reg_list));
			if ((!cli_get_str("REPLINSTANCE", fbuff, &flen)) || (0 == flen))
			{
				util_out_print("Error parsing REPLINSTANCE qualifier", TRUE);
				mupip_exit(ERR_MUPCLIERR);
			}
			if (FALSE == mubgetfil(mu_repl_inst_reg_list, fbuff, flen))
				return;
			/* Do not let the db region backup destination list be affected if -replinstance had directory specified */
			is_directory = FALSE;
		}
		if ((0 == TREF(parms_cnt)) && mu_repl_inst_reg_list)
		{	/* -REPLINSTANCE was specified and no other parameters were specified. Do not bother prompting
			 * the user to enter values for REG_NAME and SAVE_DIR parameters. */
			return;
		}
		if (!cli_get_str(name, rbuff, &rlen))
			mupip_exit(ERR_MUNODBNAME);
		for (i = 0; i < rlen; i++)
			rbuff[i] = TOUPPER(rbuff[i]); /* Region names are always upper-case ASCII and thoroughly NUL terminated */
		for ( ; i < ARRAYSIZE(rbuff); i++)
			rbuff[i] = 0;
		flen = SIZEOF(fbuff);	/* reset max_buflen to original before call to "cli_get_str" */
		if ((!cli_get_str("SAVE_DIR", fbuff, &flen)) || (0 == flen))
			mupip_exit(ERR_MUBCKNODIR);
	}
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
				is_statsDB = IS_STATSDB_REG(reg);
				is_autoDB = IS_AUTODB_REG(reg);
				/* See if autoDB file actually exists to know whether to include this region in the list or not.
				 * But a statsDB (which is also an autoDB) region does not point to the real statsdb file name.
				 * (will be determined only when the baseDB is opened). So skip this check for statsDBs.
				 * LOG_ERROR_FALSE usage below as we do not want error message displayed if file does not exist.
				 */
				if (!is_statsDB && is_autoDB && !mupfndfil(reg, NULL, LOG_ERROR_FALSE))
					continue;	/* autoDB does not exist - do not include */
				if (IS_STATSDB_REGNAME(reg))
				{	/* This is a statsdb region. Since the statsdb file name is not determined until the
					 * basedb region is opened, we cannot run "insert_region" on this region. Skip for now.
					 * Caller needs to do the needful for statsdb regions since it is the one that will
					 * do a "gvcst_init" of the baseDB region.
					 */
					continue;
				}
				if (TRUE == str_match((char *)REG_STR_LEN(reg), c1, c2 - c1))
				{
					matched = TRUE;
					if (NULL == (list = insert_region(reg, &(grlist), NULL, size)))
					{
						error_mupip = TRUE;
						gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, REG_LEN_STR(reg));
						continue;
					}
					if ((FALSE == in_backup) || (0 != ((backup_reg_list *)list)->backup_file.len))
						continue;
					if (TRUE == is_directory)
						mubexpfilnam(directory.addr, directory.len, (backup_reg_list *)list);
					else
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
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_NOREGION, 2, c2 - c1, c1);
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
