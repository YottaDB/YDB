/****************************************************************
 *								*
 *	Copyright 2003, 2004 Sanchez Computer Associates, Inc.	*
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

#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "hashdef.h"
#include "buddy_list.h"
#include "jnl.h"
#include "muprec.h"
#include "gtmio.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "gtm_rename.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	jnl_ctl_list	*mur_jctl;
GBLREF	int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);

int4 mur_cre_file_extfmt(int recstat)
{
	fi_type			*file_info;
	char			*ptr, rename_fn[MAX_FN_LEN];
	int			rename_fn_len, base_len, fn_exten_size;
	uint4			status;
	mval			op_val, op_pars;
	static readonly	char 	*fn_exten[] = {EXT_MJF, EXT_BROKEN, EXT_LOST};
	static readonly	char 	*ext_file_type[] = {STR_JNLEXTR, STR_BRKNEXTR, STR_LOSTEXTR};
	static readonly unsigned char		open_params_list[]=
	{
		(unsigned char)iop_newversion,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_eol
	};
	error_def(ERR_FILENOTCREATE);
	error_def(ERR_FILECREATE);

	assert(GOOD_TN == recstat || BROKEN_TN == recstat || LOST_TN == recstat);
	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);
	assert(GOOD_TN != recstat || mur_options.extr[GOOD_TN]);
	ptr = (char *)&mur_jctl->jnl_fn[mur_jctl->jnl_fn_len];
	while (DOT != *ptr)	/* we know journal file name alway has a DOT */
		ptr--;
	base_len = ptr - (char *)&mur_jctl->jnl_fn[0];
	file_info = murgbl.file_info[recstat] = (void *)malloc(sizeof(fi_type));
	if (0 == mur_options.extr_fn_len[recstat])
	{
		mur_options.extr_fn[recstat] = malloc(MAX_FN_LEN);
		mur_options.extr_fn_len[recstat] = base_len;
		memcpy(mur_options.extr_fn[recstat], mur_jctl->jnl_fn, base_len);
		fn_exten_size = strlen(fn_exten[recstat]);
		memcpy(mur_options.extr_fn[recstat] + base_len, fn_exten[recstat], fn_exten_size);
		mur_options.extr_fn_len[recstat] += fn_exten_size;
	}
	file_info->fn_len = mur_options.extr_fn_len[recstat];
	file_info->fn = mur_options.extr_fn[recstat];
	if (RENAME_FAILED == rename_file_if_exists(file_info->fn, file_info->fn_len, rename_fn, &rename_fn_len, &status))
		return status;
	op_pars.mvtype = MV_STR;
	op_pars.str.len = sizeof(open_params_list);
	op_pars.str.addr = (char *)open_params_list;
	op_val.mvtype = MV_STR;
	op_val.str.len = file_info->fn_len;
	op_val.str.addr = (char *)file_info->fn;
	if ((status = (*op_open_ptr)(&op_val, &op_pars, 0, NULL)) == 0)
	{
		gtm_putmsg(VARLSTCNT(5) ERR_FILENOTCREATE, 2, file_info->fn_len, file_info->fn, errno);
		return ERR_FILENOTCREATE;
	}
	/* Write file version info for the file created here. See C9B08-001729 */
	if (!mur_options.detail)
	{
		memcpy(murgbl.extr_buff, JNL_EXTR_LABEL, sizeof(JNL_EXTR_LABEL) - 1);
		jnlext_write(file_info, murgbl.extr_buff, sizeof(JNL_EXTR_LABEL));
	} else
	{
		memcpy(murgbl.extr_buff, JNL_DET_EXTR_LABEL, sizeof(JNL_DET_EXTR_LABEL) - 1);
		jnlext_write(file_info, murgbl.extr_buff, sizeof(JNL_DET_EXTR_LABEL));
	}
	gtm_putmsg(VARLSTCNT(6) ERR_FILECREATE, 4, LEN_AND_STR(ext_file_type[recstat]), file_info->fn_len, file_info->fn);
	return SS_NORMAL;
}
