/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <ssdef.h>
#include <fab.h>
#include <rms.h>
#include <iodef.h>
#include <descrip.h>

#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "buddy_list.h"
#include "jnl.h"
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "gtmio.h"
#include "gtmmsg.h"
#include "gtm_rename.h"

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;

/* VMS does not need rename if file name matches ??? */
int4 mur_cre_file_extfmt(jnl_ctl_list *jctl, int recstat)
{
	fi_type			*file_info;
	struct  FAB             *fab_ptr;
	struct  NAM             nam;
	uint4			status;
	int			base_len, rename_fn_len, fn_exten_size;
	char                    fn_buffer[MAX_FN_LEN], *ptr;
	static readonly	char 	*fn_exten[] = {EXT_MJF, EXT_BROKEN, EXT_LOST};
	static readonly	char 	*ext_file_type[] = {STR_JNLEXTR, STR_BRKNEXTR, STR_LOSTEXTR};

	error_def(ERR_FILENOTCREATE);
	error_def(ERR_FILEPARSE);
	error_def(ERR_FILECREATE);
	error_def(ERR_TEXT);

	assert(GOOD_TN == recstat || BROKEN_TN == recstat || LOST_TN == recstat);
	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);
	assert(GOOD_TN != recstat || mur_options.extr[GOOD_TN]);
	ptr = &jctl->jnl_fn[jctl->jnl_fn_len];
	while (DOT != *ptr)	/* we know journal file name alway has a DOT */
		ptr--;
	base_len = ptr - (char *)&jctl->jnl_fn[0];
	file_info = murgbl.file_info[recstat] = (void *)malloc(SIZEOF(fi_type));
	fab_ptr = file_info->fab = (struct FAB *)malloc(SIZEOF(struct FAB));
	*fab_ptr = cc$rms_fab;
	fn_exten_size = strlen(fn_exten[recstat]);
	if (0 == mur_options.extr_fn_len[recstat])
	{
		mur_options.extr_fn[recstat] = malloc(MAX_FN_LEN);
		mur_options.extr_fn_len[recstat] = base_len;
		memcpy(mur_options.extr_fn[recstat], jctl->jnl_fn, base_len);
		memcpy(mur_options.extr_fn[recstat] + base_len, fn_exten[recstat], fn_exten_size);
		mur_options.extr_fn_len[recstat] += fn_exten_size;
	}
	if (RENAME_FAILED == rename_file_if_exists(mur_options.extr_fn[recstat], mur_options.extr_fn_len[recstat],
		fn_buffer, &rename_fn_len, &status))
		return status;
	fab_ptr->fab$b_fns = mur_options.extr_fn_len[recstat];
	fab_ptr->fab$l_fna = mur_options.extr_fn[recstat];
	fab_ptr->fab$b_dns = fn_exten_size;
	fab_ptr->fab$l_dna = fn_exten[recstat];
	fab_ptr->fab$l_nam = &nam;
	nam = cc$rms_nam;
	nam.nam$b_ess = SIZEOF(fn_buffer);
	nam.nam$l_esa = fn_buffer;
	nam.nam$b_nop |= NAM$M_SYNCHK;
	file_info->rab = (struct RAB *)malloc(SIZEOF(struct RAB));
	*file_info->rab = cc$rms_rab;
	file_info->rab->rab$l_fab = fab_ptr;
	file_info->rab->rab$l_rop = RAB$M_WBH;
	status = sys$parse(fab_ptr);
	if (!(1 & status))
	{
		gtm_putmsg(VARLSTCNT(6) ERR_FILEPARSE, 2, fab_ptr->fab$b_fns, fab_ptr->fab$l_fna, status, fab_ptr->fab$l_stv);
		return status;
	}
	if (0 != nam.nam$b_node)
	{
		gtm_putmsg(VARLSTCNT(8) ERR_FILEPARSE, 2, fab_ptr->fab$b_fns, fab_ptr->fab$l_fna, ERR_TEXT, 2,
			LEN_AND_LIT("Cannot open lost transactions file across network"));
		return ERR_FILEPARSE;
	}
	fab_ptr->fab$l_nam = NULL;
	fab_ptr->fab$w_mrs = 32767;
	fab_ptr->fab$b_rat = FAB$M_CR;
	fab_ptr->fab$l_fop = FAB$M_CBT | FAB$M_MXV;
	fab_ptr->fab$b_fac = FAB$M_PUT;
	status = sys$create(fab_ptr);
	if (1 & status)
	{
		status = sys$connect(file_info->rab);
		if (!(1 & status))
			sys$close(fab_ptr);	/* use sys$close() if FAB$M_UFO was not specified in fab$l_fop in open */
	}
	if (!(1 & status))
	{
		gtm_putmsg(VARLSTCNT(6) ERR_FILENOTCREATE, 2, fab_ptr->fab$b_fns, fab_ptr->fab$l_fna, status, fab_ptr->fab$l_stv);
		return status;
	}
	/* Write file version info for the file created here. See C9B08-001729 */
	if (!mur_options.detail)
	{
		MEMCPY_LIT(murgbl.extr_buff, JNL_EXTR_LABEL);
		jnlext_write(file_info, murgbl.extr_buff, SIZEOF(JNL_EXTR_LABEL));
	} else
	{
		MEMCPY_LIT(murgbl.extr_buff, JNL_DET_EXTR_LABEL);
		jnlext_write(file_info, murgbl.extr_buff, SIZEOF(JNL_DET_EXTR_LABEL));
	}
	gtm_putmsg(VARLSTCNT(6) ERR_FILECREATE, 4, LEN_AND_STR(ext_file_type[recstat]), fab_ptr->fab$b_fns, fab_ptr->fab$l_fna);
	return SS$_NORMAL;
}
