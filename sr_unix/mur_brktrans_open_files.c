/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <netinet/in.h> /* Required for gtmsource.h */
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "gtm_stat.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_stdlib.h"

#include "cli.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "jnl.h"
#include "muprec.h"
#include "io.h"
#include "iosp.h"
#include "copy.h"
#include "gtmio.h"
#include "gdskill.h"
#include "collseq.h"
#include "gdscc.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "parse_file.h"
#include "lockconst.h"
#include "aswp.h"
#include "eintr_wrappers.h"
#include "io_params.h"
#include "rename_file_if_exists.h"
#include "gbldirnam.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmrecv.h"
#include "util.h"
#include "gtmmsg.h"

GBLREF	mur_opt_struct	mur_options;
GBLREF	int		mur_extract_bsize;
GBLREF	char		*mur_extract_buff;
GBLREF	int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);


bool mur_brktrans_open_files(ctl_list *ctl)
{
	int			i, fn_len, rename_len, local_errno;
	int4			info_status;
	char			*c, *c1, *ctop, fn[MAX_FN_LEN], rename_fn[MAX_FN_LEN];
	fi_type                 *brktrans_file_info;
        jnl_file_header         *header;
	uint4			status;
	mval			op_val, op_pars;
	static readonly unsigned char		open_params_list[8]=
	{
		(unsigned char)iop_newversion,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_recordsize, (unsigned char)0x07F,(unsigned char)0x07F,
		(unsigned char)iop_eol
	};

	error_def(ERR_RENAMEFAIL);

	if (mur_options.brktrans_file_info == NULL)
        {
        	brktrans_file_info = (fi_type *)(mur_options.brktrans_file_info = (void *)malloc(sizeof(fi_type)));
                c = ctl->jnl_fn;
                ctop = c + ctl->jnl_fn_len;
                c1 = fn;
                while (c < ctop  &&  *c != '.')
                        *c1++ = *c++;
                strcpy(c1, ".broken");
		fn_len = strlen(fn);
                brktrans_file_info->fn = (char *)(malloc(fn_len + 1));
                strcpy(brktrans_file_info->fn, fn);
                brktrans_file_info->fn_len = fn_len;
                header = mur_get_file_header(ctl->rab);
		if (header->max_record_length > mur_extract_bsize)
			 mur_extract_bsize = header->max_record_length;
		if (mur_extract_bsize == 0)
			mur_extract_bsize = 10240; /* Hard coded ??? */
		if (RENAME_FAILED == rename_file_if_exists(brktrans_file_info->fn, brktrans_file_info->fn_len,
                        &info_status, rename_fn, &rename_len))
                        gtm_putmsg(VARLSTCNT(7) ERR_RENAMEFAIL, 5, brktrans_file_info->fn_len,
                                brktrans_file_info->fn, rename_len, rename_fn, info_status);
		op_pars.mvtype = MV_STR;
		op_pars.str.len = sizeof(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.mvtype = MV_STR;
		op_val.str.len = brktrans_file_info->fn_len;
		op_val.str.addr = (char *)brktrans_file_info->fn;
		if((status = (*op_open_ptr)(&op_val, &op_pars, 0, 0)) == 0)
		{
			gtm_putmsg(VARLSTCNT(1) errno);
			util_out_print("Error opening Broken trans file !AD ", TRUE,
						brktrans_file_info->fn_len, brktrans_file_info->fn);
			return FALSE;
		}
		mur_extract_buff = (char *)malloc(mur_extract_bsize + 256);
	}
	return TRUE;
}
