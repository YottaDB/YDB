/****************************************************************
 *								*
 * Copyright (c) 2003-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#if defined(UNIX)
#include "gtm_unistd.h"
#elif defined(VMS)
#include <rms.h>
#include <iodef.h>
#include <psldef.h>
#include <ssdef.h>
#include <efndef.h>
#include "iosb_disk.h"
#endif
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "buddy_list.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"
#include "read_db_files_from_gld.h"
#include "mu_gv_cur_reg_init.h"
#include "mur_db_files_from_jnllist.h"
#include "mur_read_file.h"
#include "iosp.h"
#include "gtmio.h"
#include "gtmmsg.h"

GBLREF	gd_region	*gv_cur_region;
GBLREF	gd_addr		*gd_header;

error_def(ERR_JNLREAD);
error_def(ERR_PREMATEOF);
error_def(ERR_FILEPARSE);

gld_dbname_list *mur_db_files_from_jnllist(char *jnl_file_list, unsigned short jnl_file_list_len, int *db_total)
{
	gd_region		*reg, *reg_start, *reg_top;
	gd_segment 		*seg;
	int			db_tot;
	unsigned int		db_fname_len;
	uint4			ustatus;
	gld_dbname_list		head, *tdblist, *dblist = &head;
	char 			*cptr, *ctop, *cptr_last, db_fname[GTM_PATH_MAX];
	jnl_ctl_list		jctl_temp, *jctl = &jctl_temp;
	jnl_file_header		*jfh;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif

	db_tot = 0;
	tdblist = head.next = NULL;
	cptr = jnl_file_list;
	ctop = &jnl_file_list[jnl_file_list_len];
	memset(&jctl_temp, 0, SIZEOF(jctl_temp));
	if (NULL != gd_header)
	{	/* global directory has already been opened by a "gvinit" in mur_open_files (for recover or rollback). */
		reg_start = gd_header->regions;
		reg_top = reg_start + gd_header->n_regions;
	} else
	{	/* Do not use global directory regions. Create regions using mu_gv_cur_reg_init from dbname in jnl file header */
		reg_start = NULL;
		reg_top = NULL;
	}
	/* Get full path of db file names pointed to by regions in global directory.
	 * This is needed for later comparison with db file names in journal file header.
	 */
	for (reg = reg_start; reg < reg_top; reg++)
	{
		seg = reg->dyn.addr;
		if (!get_full_path((char *)seg->fname, (unsigned int)seg->fname_len,
					(char *)&db_fname[0], &db_fname_len, GTM_PATH_MAX, &ustatus))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, seg->fname_len, seg->fname, ustatus);
			return NULL;
		}
		assert(db_fname_len && (db_fname_len < MAX_FN_LEN + 1));
		seg->fname_len = db_fname_len;
		memcpy(seg->fname, &db_fname[0], db_fname_len);
		/* This code is lifted from the tail of "mu_gv_cur_reg_init". Any changes here need to be reflected there */
		FILE_CNTL_INIT(seg);
	}
	while (cptr < ctop)
	{
		cptr_last = cptr;
		while (0 != *cptr && ',' != *cptr && '"' != *cptr &&  ' ' != *cptr)
			++cptr;
		if (!get_full_path(cptr_last, (unsigned int)(cptr - cptr_last),
					(char *)jctl->jnl_fn, &jctl->jnl_fn_len, MAX_FN_LEN, &ustatus))
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_FILEPARSE, 2, cptr - cptr_last, cptr_last, ustatus);
			return NULL;
		}
		cptr++;	/* skip separator */
		if (SS_NORMAL != mur_fopen_sp(jctl, NULL))
			return NULL;
		jctl->jfh = (jnl_file_header *)malloc(REAL_JNL_HDR_LEN);
		jfh = jctl->jfh;
		DO_FILE_READ(jctl->channel, 0, jfh, REAL_JNL_HDR_LEN, jctl->status, jctl->status2);
		if (SS_NORMAL != jctl->status) /* read fails */
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0, jctl->status);
			/* should we do mur_fclose(jctl) here ? */
			return NULL;
		}
		CHECK_JNL_FILE_IS_USABLE(jfh, jctl->status, TRUE, jctl->jnl_fn_len, jctl->jnl_fn);
		if (SS_NORMAL != jctl->status)
			/* should we do mur_fclose(jctl) here ? */
			return NULL;	/* gtm_putmsg would have already been done by CHECK_JNL_FILE_IS_USABLE macro */
		/* Check if db file name in journal file header points to any region in already allocated list */
		for (tdblist = head.next; NULL != tdblist; tdblist = tdblist->next)
		{	/* We cannot call is_file_identical because file may not really exist */
			seg = tdblist->gd->dyn.addr;
			if ((seg->fname_len == jfh->data_file_name_length)
					&& !memcmp(seg->fname, jfh->data_file_name, seg->fname_len))
				break;	/* match found */
		}
		if (NULL == tdblist)
		{	/* Did not find any existing db structure to match this jnl file. Allocate one. */
			/* Check if db file name in journal file header points to any region in the current global directory.
			 * If so use that region structure. If not, allocate one.
			 */
			for (reg = reg_start; reg < reg_top; reg++)
			{
				seg = reg->dyn.addr;
				if ((seg->fname_len == jfh->data_file_name_length)
						&& !memcmp(seg->fname, jfh->data_file_name, seg->fname_len))
					break; /* Found db in gld file. Use that region structure. */
			}
			if (reg == reg_top)
			{	/* Could not find db in gld file or empty gld file. Allocate region structure */
				mu_gv_cur_reg_init();
				reg = gv_cur_region;
				seg = (gd_segment *)reg->dyn.addr;
				seg->fname_len = jfh->data_file_name_length;
				if (seg->fname_len >= SIZEOF(seg->fname))
					seg->fname_len = SIZEOF(seg->fname) - 1;
				memcpy(seg->fname, jfh->data_file_name, seg->fname_len);
				seg->fname[seg->fname_len] = 0;
			}
			dblist = dblist->next = (gld_dbname_list *)malloc(SIZEOF(gld_dbname_list));
			memset(dblist, 0, SIZEOF(gld_dbname_list));
			reg->stat.addr = (void *)dblist; /* is it necessary ??? */
			dblist->gd = reg;
			db_tot++;
		}
		/* else : multiple journal files for same database. continue processing */
		if (!mur_fclose(jctl))	/* cannot do this until "jfh" usages are done above */
			return NULL;
	}
	*db_total = db_tot;
	return head.next;
}
