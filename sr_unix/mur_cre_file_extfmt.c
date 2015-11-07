/****************************************************************
 *								*
 *	Copyright 2003, 2013 Fidelity Information Services, Inc	*
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
#include "gtm_strings.h"

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
GBLREF	jnlpool_addrs	jnlpool;
GBLREF	int		(*op_open_ptr)(mval *v, mval *p, int t, mval *mspace);

error_def(ERR_FILENOTCREATE);
error_def(ERR_FILECREATE);

int4 mur_cre_file_extfmt(jnl_ctl_list *jctl, int recstat)
{
	fi_type			*file_info;
	char			*ptr, rename_fn[MAX_FN_LEN];
	int			rename_fn_len, base_len, fn_exten_size, extrlen, tmplen;
	uint4			status;
	mval			op_val, op_pars;
	boolean_t		is_stdout;	/* Output will go STDOUT? */
	static readonly	char 	*fn_exten[] = {EXT_MJF, EXT_BROKEN, EXT_LOST};
	static readonly	char 	*ext_file_type[] = {STR_JNLEXTR, STR_BRKNEXTR, STR_LOSTEXTR};
	static readonly unsigned char		open_params_list[]=
	{
		(unsigned char)iop_m,
		(unsigned char)iop_newversion,
		(unsigned char)iop_noreadonly,
		(unsigned char)iop_nowrap,
		(unsigned char)iop_stream,
		(unsigned char)iop_eol
	};

	assert(GOOD_TN == recstat || BROKEN_TN == recstat || LOST_TN == recstat);
	assert(0 == GOOD_TN);
	assert(1 == BROKEN_TN);
	assert(2 == LOST_TN);
	assert(GOOD_TN != recstat || mur_options.extr[GOOD_TN]);
	/* Argument journal -extract=-stdout ? */
	is_stdout = mur_options.extr_fn[recstat]
		&& (0 == STRNCASECMP(mur_options.extr_fn[recstat], JNL_STDO_EXTR, SIZEOF(JNL_STDO_EXTR)));
	/* If we need to write to stdout, we can bypass file renaming stuff */
	if(!is_stdout)
	{
		ptr = (char *)&jctl->jnl_fn[jctl->jnl_fn_len];
		while (DOT != *ptr)	/* we know journal file name alway has a DOT */
			ptr--;
		base_len = (int)(ptr - (char *)&jctl->jnl_fn[0]);
		file_info = (void *)malloc(SIZEOF(fi_type));
		if (0 == mur_options.extr_fn_len[recstat])
		{
			mur_options.extr_fn[recstat] = malloc(MAX_FN_LEN);
			mur_options.extr_fn_len[recstat] = base_len;
			memcpy(mur_options.extr_fn[recstat], jctl->jnl_fn, base_len);
			fn_exten_size = STRLEN(fn_exten[recstat]);
			memcpy(mur_options.extr_fn[recstat] + base_len, fn_exten[recstat], fn_exten_size);
			mur_options.extr_fn_len[recstat] += fn_exten_size;
		}
		file_info->fn_len = mur_options.extr_fn_len[recstat];
		file_info->fn = mur_options.extr_fn[recstat];
		murgbl.file_info[recstat] = file_info;
		if (RENAME_FAILED == rename_file_if_exists(file_info->fn, file_info->fn_len, rename_fn, &rename_fn_len, &status))
			return status;
		op_pars.mvtype = MV_STR;
		op_pars.str.len = SIZEOF(open_params_list);
		op_pars.str.addr = (char *)open_params_list;
		op_val.mvtype = MV_STR;
		op_val.str.len = file_info->fn_len;
		op_val.str.addr = (char *)file_info->fn;
		if ((status = (*op_open_ptr)(&op_val, &op_pars, 0, NULL)) == 0)
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILENOTCREATE, 2, file_info->fn_len, file_info->fn, errno);
			return ERR_FILENOTCREATE;
		}
	}
	/* Write file version info for the file created here. See C9B08-001729 */
	if (!mur_options.detail)
	{
		MEMCPY_LIT(murgbl.extr_buff, JNL_EXTR_LABEL);
		extrlen = STR_LIT_LEN(JNL_EXTR_LABEL);
	} else
	{
		MEMCPY_LIT(murgbl.extr_buff, JNL_DET_EXTR_LABEL);
		extrlen = STR_LIT_LEN(JNL_DET_EXTR_LABEL);
	}
	if (LOST_TN == recstat)
	{
		if (mur_options.update)
		{
			if (mur_options.rollback)
				ptr = " ROLLBACK";
			else
				ptr = " RECOVER";
		} else
			ptr = " EXTRACT";
		tmplen = STRLEN(ptr);
		memcpy(&murgbl.extr_buff[extrlen], ptr, tmplen);
		extrlen += tmplen;
		if (mur_options.rollback)
		{
			if (mur_options.fetchresync_port && murgbl.was_rootprimary)
				ptr = " PRIMARY ";
			else
				ptr = " SECONDARY ";
			tmplen = STRLEN(ptr);
			memcpy(&murgbl.extr_buff[extrlen], ptr, tmplen);
			extrlen += tmplen;
			assert(NULL != jnlpool.repl_inst_filehdr);
			ptr = (char *)&jnlpool.repl_inst_filehdr->inst_info.this_instname[0];
			tmplen = STRLEN(ptr);
			memcpy(&murgbl.extr_buff[extrlen], ptr, tmplen);
			extrlen += tmplen;
		}
	}
	if (gtm_utf8_mode)
	{
		murgbl.extr_buff[extrlen++] = ' ';
		MEMCPY_LIT(&murgbl.extr_buff[extrlen], UTF8_NAME);
		extrlen += STR_LIT_LEN(UTF8_NAME);
	}
	murgbl.extr_buff[extrlen++] = '\\';
	jnlext_write((is_stdout ? NULL : file_info), murgbl.extr_buff, extrlen);
	if (!is_stdout) /* We wrote to stdout so it doesn't make a sense to print a message about file creation. */
		gtm_putmsg(VARLSTCNT(6) ERR_FILECREATE, 4, LEN_AND_STR(ext_file_type[recstat]), file_info->fn_len, file_info->fn);
	return SS_NORMAL;
}
