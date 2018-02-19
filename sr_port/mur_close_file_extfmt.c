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
#include "gtm_stdio.h"
#include "gtm_strings.h"

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "muprec.h"

#include "gtmio.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "gtm_rename.h"

# define	MUR_CLOSE_FILE(file_info)				\
{									\
	mval			val, pars;				\
	unsigned char		no_param;				\
									\
	no_param = (unsigned char)iop_eol;				\
	pars.mvtype = MV_STR;						\
	pars.str.len = SIZEOF(no_param);				\
	pars.str.addr = (char *)&no_param;				\
	val.mvtype = MV_STR;						\
	val.str.len = ((unix_file_info *)file_info)->fn_len;		\
	val.str.addr = (char *) (((unix_file_info *)(file_info))->fn);	\
	if (NULL == val.str.addr)					\
		continue;						\
	op_close(&val, &pars);						\
}

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	reg_ctl_list	*mur_ctl;

error_def(ERR_FILENOTCREATE);

void mur_close_file_extfmt(boolean_t in_mur_close_files)
{
	int		recstat;
	fi_type		*file_info;
	reg_ctl_list	*rctl, *rctl_top;
	boolean_t	extr_file_created, this_reg_file_created;
	char		*fn;
	static readonly	char 	*ext_file_type[] = {STR_JNLEXTR, STR_BRKNEXTR, STR_LOSTEXTR};

	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);

	for (recstat = 0; recstat < TOT_EXTR_TYPES; recstat++)
	{
		extr_file_created = FALSE;
		for (rctl = mur_ctl, rctl_top = mur_ctl + murgbl.reg_total; rctl < rctl_top; rctl++)
		{
			this_reg_file_created = rctl->extr_file_created[recstat] && (NULL != rctl->file_info[recstat]);
			if (this_reg_file_created)
				extr_file_created = TRUE;
			if (!in_mur_close_files)
			{
				if (NULL != rctl->file_info[recstat])
					MUR_CLOSE_FILE(rctl->file_info[recstat]);
			} else if (this_reg_file_created && !murgbl.clean_exit)
			{	/* This is not a normal exit of MUPIP JOURNAL. Delete any files that we created.
				 * If "multi_proc_in_use" was TRUE in the forward phase, "mur_merge_sort_extfmt"
				 * would have taken care of this cleanup. We need the below for the FALSE case.
				 */
				fn = ((fi_type *)rctl->file_info[recstat])->fn;
				MUR_JNLEXT_UNLINK(fn);
			}
			/* Note: Not freeing up rctl->file_info[recstat] here as the process is about to die anyways */
		}
		if (in_mur_close_files && !extr_file_created && !murgbl.filenotcreate_displayed[recstat])
		{	/* If STDOUT no file closing message. */
			if (mur_options.extr_fn[recstat] && !mur_options.extr_fn_is_stdout[recstat])
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_FILENOTCREATE, 4, LEN_AND_STR(ext_file_type[recstat]),
							mur_options.extr_fn_len[recstat], mur_options.extr_fn[recstat]);
		}
	}
}
