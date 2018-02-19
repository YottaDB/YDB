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
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "iosp.h"
#include "gtmmsg.h"
#include "gtm_rename.h"
#include "repl_instance.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtm_utf8.h"
#include "gtm_multi_proc.h"
#include "interlock.h"

/* This function creates a file to hold either journal extract or broken transaction or lost transaction data.
 * The headerline of the file created will contain one of the following
 *
 *	Created by MUPIP JOURNAL -EXTRACT  --> EXTRACTLABEL<sp>EXTRACT
 *	Created by MUPIP JOURNAL -RECOVER  --> EXTRACTLABEL<sp>RECOVER
 *	Created by MUPIP JOURNAL -ROLLBACK --> EXTRACTLABEL<sp>ROLLBACK<sp>[PRIMARY|SECONDARY]<sp>INSTANCENAME
 *
 *	EXTRACTLABEL = NORMAL-EXTRACT-LABEL | DETAILED-EXTRACT-LABEL	// see muprec.h
 *	<sp> : Space
 *	RECOVER, EXTRACT, ROLLBACK, PRIMARY, SECONDARY : literal strings that appear as is.
 *	INSTANCENAME : Name of the replication instance
 */

GBLREF 	mur_gbls_t	murgbl;
GBLREF	mur_opt_struct	mur_options;
GBLREF	int		(*op_open_ptr)(mval *v, mval *p, mval *t, mval *mspace);
GBLREF	mur_shm_hdr_t	*mur_shm_hdr;	/* Pointer to mur_forward-specific header in shared memory */
GBLREF	reg_ctl_list	*mur_ctl;
GBLREF	readonly char 	*ext_file_type[];

LITREF	mval		literal_zero;

error_def(ERR_FILECREATE);
error_def(ERR_FILENAMETOOLONG);
error_def(ERR_FILENOTCREATE);

int4 mur_cre_file_extfmt(jnl_ctl_list *jctl, int recstat)
{
	fi_type			*file_info;
	char			*ptr, rename_fn[MAX_FN_LEN + 1];
	int			rename_fn_len, base_len, fn_exten_size, tmplen, rctl_index;
	uint4			status;
	mval			op_val, op_pars;
	boolean_t		is_stdout;	/* Output will go STDOUT?. Matters only for single-region in this function */
	boolean_t		need_rel_latch, copy_from_shm, single_reg, release_latch, key_reset;
	boolean_t		is_dummy_gbldir;
	reg_ctl_list		*rctl;
	gd_region		*reg;
	shm_reg_ctl_t		*shm_rctl_start, *shm_rctl;
#	ifdef DEBUG
	unsigned char		*tmp_key;
#	endif
	static readonly	char 	*fn_exten[] = {EXT_MJF, EXT_BROKEN, EXT_LOST};
	static readonly unsigned char	open_params_list[]=
	{
		(unsigned char)iop_m,
		(unsigned char)iop_newversion,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_buffered, 1, 0x03,
		(unsigned char)iop_eol
	};

	assert(GOOD_TN == recstat || BROKEN_TN == recstat || LOST_TN == recstat);
	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);
	assert(GOOD_TN != recstat || mur_options.extr[GOOD_TN]);
	need_rel_latch = FALSE;
	single_reg = (1 == murgbl.reg_total);
	rctl = jctl->reg_ctl;
	if (multi_proc_in_use)
	{	/* Determine if some other parallel process has already created this file. If so, use that. If not create one */
		assert(!single_reg);
		GRAB_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		assert(release_latch);
		if ('\0' == mur_shm_hdr->extr_fn[recstat].fn[0])
		{	/* No one created this file. Let us first determine the name of the extract file we need to create.
			 * This is needed so each parallel process can create a temporary extract file based on this name.
			 */
			need_rel_latch = TRUE;
		} else
		{
			/* We know mur_shm_hdr->extr_fn[recstat] has been completely initialized. And it is not touched anymore.
			 * So we can read it without the latch. Release it now and read file name later.
			 */
			REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
		}
		if (NULL == multi_proc_key)
		{
			MUR_SET_MULTI_PROC_KEY(rctl, multi_proc_key);
			key_reset = TRUE;
		} else
		{	/* Assert that key is already set to the right value */
#			ifdef DEBUG
			MUR_SET_MULTI_PROC_KEY(rctl, tmp_key);
			assert(tmp_key == multi_proc_key);
#			endif
			key_reset = FALSE;
		}
	}
	is_stdout = single_reg && mur_options.extr_fn[recstat] && mur_options.extr_fn_is_stdout[recstat];
	if (!is_stdout)
	{
		file_info = (void *)malloc(SIZEOF(fi_type));
		file_info->fn = malloc(MAX_FN_LEN);
		if (0 == mur_options.extr_fn_len[recstat])
		{
			if (!multi_proc_in_use || need_rel_latch)
			{
				mur_options.extr_fn[recstat] = malloc(MAX_FN_LEN);
				ptr = (char *)&jctl->jnl_fn[jctl->jnl_fn_len];
				while (DOT != *ptr)	/* we know journal file name always has a DOT */
					ptr--;
				base_len = (int)(ptr - (char *)&jctl->jnl_fn[0]);
				mur_options.extr_fn_len[recstat] = base_len;
				memcpy(mur_options.extr_fn[recstat], jctl->jnl_fn, base_len);
				fn_exten_size = STRLEN(fn_exten[recstat]);
				memcpy(mur_options.extr_fn[recstat] + base_len, fn_exten[recstat], fn_exten_size);
				mur_options.extr_fn_len[recstat] += fn_exten_size;
				copy_from_shm = FALSE;
			} else
			{	/* Copy extract file name that has already been determined by another process */
				copy_from_shm = TRUE;
			}
		} else
			copy_from_shm = FALSE;
		if (!copy_from_shm)
		{
			memcpy(file_info->fn, mur_options.extr_fn[recstat], mur_options.extr_fn_len[recstat]);
			file_info->fn_len = mur_options.extr_fn_len[recstat];
			assert(!multi_proc_in_use || need_rel_latch
				|| !memcmp(mur_shm_hdr->extr_fn[recstat].fn, file_info->fn, file_info->fn_len));
			if (need_rel_latch)
			{	/* Now that the extract file name has been determined, copy this over to shared memory */
				memcpy(mur_shm_hdr->extr_fn[recstat].fn, file_info->fn, file_info->fn_len);
				mur_shm_hdr->extr_fn_len[recstat] = file_info->fn_len;
				REL_MULTI_PROC_LATCH_IF_NEEDED(release_latch);
			}
		} else
		{
			assert(!need_rel_latch);
			memcpy(file_info->fn, mur_shm_hdr->extr_fn[recstat].fn, mur_shm_hdr->extr_fn_len[recstat]);
			file_info->fn_len = mur_shm_hdr->extr_fn_len[recstat];
		}
		rctl->file_info[recstat] = file_info;
		rctl->extr_fn_len_orig[recstat] = file_info->fn_len;
		/* Now adjust the file name to be region-specific. Add a region-name suffix. If no region-name is found,
		 * add region #. Do this only if there are at least 2 regions. Otherwise no need of a merge sort.
		 */
		if (!single_reg)
		{
			reg = rctl->gd;
			/* Calculate if appending region name will not overflow allocation. If so error out */
			tmplen = file_info->fn_len;
			tmplen++;	/* for the '_' */
			/* If this region corresponds to a gld created by "gd_load" then it is a real gld and use that
			 * "region-name". Else it is a dummy gld created by "mu_gv_cur_reg_init" in which case use a
			 * number (of the rctl in the rctl array) to differentiate multiple journal files. Thankfully
			 * "mu_gv_cur_reg_init" uses "create_dummy_gbldir" which sets gd->link = NULL whereas "gd_load"
			 * sets it to a non-NULL value. So use that as the distinguishing characteristic.
			 */
			is_dummy_gbldir = reg->owning_gd->is_dummy_gbldir;
			if (!is_dummy_gbldir)
			{
				assert(reg->rname_len);
				tmplen += reg->rname_len;
			} else
			{	/* maximum # of regions is limited by MULTI_PROC_MAX_PROCS (since that is the limit
				 * that "gtm_multi_proc" can handle. Use the byte-length of MULTI_PROC_MAX_PROCS-1.
				 */
				assert(!memcmp(reg->rname, "DEFAULT", reg->rname_len));
				assert(1000 == MULTI_PROC_MAX_PROCS);
				tmplen += 3;	/* 999 is maximum valid value and has 3 decimal digits */
			}
			if (tmplen > MAX_FN_LEN)
			{	/* We cannot create a file. Error out */
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_FILENAMETOOLONG);
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) MAKE_MSG_ERROR(ERR_FILENOTCREATE), 4,
						LEN_AND_STR(ext_file_type[recstat]), file_info->fn_len, file_info->fn);
				return ERR_FILENOTCREATE;
			}
			tmplen = file_info->fn_len;
			ptr = &file_info->fn[tmplen];
			*ptr++ = '_'; tmplen++;
			if (!is_dummy_gbldir)
			{
				memcpy(ptr, reg->rname, reg->rname_len);
				tmplen += reg->rname_len;
			} else
				tmplen += SPRINTF(ptr, "%d", rctl - &mur_ctl[0]);
			file_info->fn_len = tmplen;
		}
		rename_fn_len = ARRAYSIZE(rename_fn);
		if (RENAME_FAILED == rename_file_if_exists(file_info->fn, file_info->fn_len, rename_fn, &rename_fn_len, &status))
			return status;
		op_pars.mvtype = MV_STR;
		op_pars.str.len = SIZEOF(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.mvtype = MV_STR;
		op_val.str.len = file_info->fn_len;
		op_val.str.addr = (char *)file_info->fn;
		if ((status = (*op_open_ptr)(&op_val, &op_pars, (mval *)&literal_zero, NULL)) == 0)
		{
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(7) MAKE_MSG_ERROR(ERR_FILENOTCREATE), 4,
					LEN_AND_STR(ext_file_type[recstat]), file_info->fn_len, file_info->fn, errno);
			if (single_reg)
				murgbl.filenotcreate_displayed[recstat] = TRUE;
			return ERR_FILENOTCREATE;
		}
	} else
		rctl->file_info[recstat] = NULL;	/* special meaning for STDOUT */
	if (single_reg)
	{
		mur_write_header_extfmt(jctl, NULL, NULL, recstat);
		/* For multi-region, this header writing will be done later as part of "mur_merge_sort_extfmt" */
		if (!is_stdout) /* We wrote to stdout so it doesn't make a sense to print a message about file creation. */
			gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_FILECREATE, 4, LEN_AND_STR(ext_file_type[recstat]),
					file_info->fn_len, file_info->fn);
	} else if (multi_proc_in_use)
	{
		if (key_reset)
			multi_proc_key = NULL;	/* reset key until it can be set to rctl's region-name again */
		/* Record the fact that this child process created an extract file in shared memory
		 * so parent can clean it up later in case the child process dies abruptly (e.g. GTM-F-MEMORY)
		 * before it does the full copy of needed information at the end of "mur_forward_multi_proc".
		 */
		rctl_index = rctl - &mur_ctl[0];
		shm_rctl_start = mur_shm_hdr->shm_rctl_start;
		shm_rctl = &shm_rctl_start[rctl_index];
		shm_rctl->extr_file_created[recstat] = TRUE;
	}
	rctl->extr_file_created[recstat] = TRUE;
	return SS_NORMAL;
}
