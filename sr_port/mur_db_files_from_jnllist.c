/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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
#include "gtm_rename.h"

GBLREF gd_region	*gv_cur_region;

gld_dbname_list *mur_db_files_from_jnllist(char *jnl_file_list, unsigned short jnl_file_list_len, int *db_total)
{
	gd_segment 		*seg;
	int			db_tot;
	uint4			ustatus;
	gld_dbname_list		head, *tdblist, *dblist = &head;
	char 			*cptr, *ctop, *cptr_last;
	jnl_ctl_list		jctl_temp, *jctl = &jctl_temp;
#if defined(VMS)
	io_status_block_disk	iosb;
#endif
	error_def(ERR_JNLREAD);
	error_def(ERR_JNLBADLABEL);
	error_def(ERR_PREMATEOF);
	error_def(ERR_FILEPARSE);

	db_tot = 0;
	tdblist = head.next = NULL;
	cptr = jnl_file_list;
	ctop = &jnl_file_list[jnl_file_list_len];
	memset(&jctl_temp, 0, sizeof(jctl_temp));
	while (cptr < ctop)
	{
		mu_gv_cur_reg_init();
		seg = (gd_segment *)gv_cur_region->dyn.addr;
		while (cptr < ctop)
		{
			cptr_last = cptr;
			while (0 != *cptr && ',' != *cptr && '"' != *cptr &&  ' ' != *cptr)
				++cptr;
			if (!get_full_path(cptr_last, (unsigned int)(cptr - cptr_last),
						(char *)jctl->jnl_fn, &jctl->jnl_fn_len, MAX_FN_LEN, &ustatus))
			{
				gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, cptr_last, cptr - cptr_last, ustatus);
				return NULL;
			}
			cptr++;	/* skip separator */
			/* Followings fix file name if system crashed during rename
			 * (directly related to cre_jnl_file_common) */
	 		cre_jnl_file_intrpt_rename(jctl->jnl_fn_len, jctl->jnl_fn);
			if (!mur_fopen_sp(jctl))
				return NULL;
			jctl->jfh = (jnl_file_header *)malloc(JNL_HDR_LEN);
			DO_FILE_READ(jctl->channel, 0, jctl->jfh, JNL_HDR_LEN, jctl->status, jctl->status2);
			if (SS_NORMAL != jctl->status) /* read fails */
			{
				gtm_putmsg(VARLSTCNT(6) ERR_JNLREAD, 3, jctl->jnl_fn_len, jctl->jnl_fn, 0, jctl->status);
				return NULL;
			}
			if (0 != MEMCMP_LIT(jctl->jfh->label, JNL_LABEL_TEXT))
			{
				gtm_putmsg(VARLSTCNT(4) ERR_JNLBADLABEL, 2, jctl->jnl_fn_len, jctl->jnl_fn);
				return NULL;
			}
			seg->fname_len = jctl->jfh->data_file_name_length;
			if (seg->fname_len >= sizeof(seg->fname))
				seg->fname_len = sizeof(seg->fname) - 1;
			memcpy(seg->fname, jctl->jfh->data_file_name, seg->fname_len);
			seg->fname[seg->fname_len] = 0;
			if (!mur_fclose(jctl))
				return NULL;
			for (tdblist = head.next; NULL != tdblist; tdblist = tdblist->next)
			{
				/* We cannot call is_file_identical because file may not really exists */
				if (tdblist->gd->dyn.addr->fname_len == seg->fname_len &&
					0 == memcmp(tdblist->gd->dyn.addr->fname, seg->fname, tdblist->gd->dyn.addr->fname_len))
					break;
			}
			if (NULL == tdblist)
				break;
			/* continue if multiple journal files for same database */
		}
		if (NULL == tdblist)
		{
			dblist = dblist->next = (gld_dbname_list *)malloc(sizeof(gld_dbname_list));
			memset(dblist, 0, sizeof(gld_dbname_list));
			gv_cur_region->stat.addr = (void *)dblist; /* is it necessary ??? */
			dblist->gd = gv_cur_region;
			db_tot++;
		} else
			mu_gv_cur_reg_free();
	}
	*db_total = db_tot;
	return head.next;
}
