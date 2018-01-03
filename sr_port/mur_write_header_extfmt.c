/****************************************************************
 *								*
 * Copyright (c) 2015-2017 Fidelity National Information	*
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
#include "hashtab_mname.h"	/* needed for muprec.h */
#include "hashtab_int4.h"	/* needed for muprec.h */
#include "hashtab_int8.h"	/* needed for muprec.h */
#include "muprec.h"
#include "repl_instance.h"
#include "repl_msg.h"
#include "gtmsource.h"
#include "gtmio.h"
#include "eintr_wrappers.h"

GBLREF 	mur_gbls_t		murgbl;
GBLREF	mur_opt_struct		mur_options;
GBLREF	jnlpool_addrs_ptr_t	jnlpool;

/* If "fp" is NULL, use "op_write" else use "GTM_FWRITE".
 * "fname" is used only if "fp" is non-NULL.
 */
void mur_write_header_extfmt(jnl_ctl_list *jctl, FILE *fp, char *fname, int recstat)
{
	char		*ptr;
	int		extrlen, save_errno, tmplen;
	size_t		ret_size;
	char		errstr[1024];

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
			{
				assert(!mur_options.forward);	/* FORWARD ROLLBACK should never generate a PRIMARY losttn file */
				ptr = " PRIMARY";
			} else
				ptr = " SECONDARY";
			tmplen = STRLEN(ptr);
			memcpy(&murgbl.extr_buff[extrlen], ptr, tmplen);
			extrlen += tmplen;
			/* For FORWARD ROLLBACK, we dont have access to the journal pool or the replication instance name.
			 * Do not write an instance name in that case.
			 */
			if (!mur_options.forward)
			{
				murgbl.extr_buff[extrlen++] = ' ';
				assert((NULL != jnlpool) && (NULL != jnlpool->repl_inst_filehdr));
				ptr = (char *)&jnlpool->repl_inst_filehdr->inst_info.this_instname[0];
				tmplen = STRLEN(ptr);
				memcpy(&murgbl.extr_buff[extrlen], ptr, tmplen);
				extrlen += tmplen;
			}
		}
	}
	if (gtm_utf8_mode)
	{
		murgbl.extr_buff[extrlen++] = ' ';
		MEMCPY_LIT(&murgbl.extr_buff[extrlen], UTF8_NAME);
		extrlen += STR_LIT_LEN(UTF8_NAME);
	}
	murgbl.extr_buff[extrlen++] = '\\';
	if (NULL == fp)
		jnlext_write(jctl, jctl->reg_ctl->mur_desc->jnlrec, recstat, murgbl.extr_buff, extrlen);
	else
	{
		assert('\\' == murgbl.extr_buff[extrlen - 1]);	/* See comment before "jnlext_write" function definition for why */
		murgbl.extr_buff[extrlen - 1] = '\n';
		GTM_FWRITE(murgbl.extr_buff, 1, extrlen, fp, ret_size, save_errno);
		if (ret_size < extrlen)
		{
			assert(FALSE);
			assert(save_errno);
			SNPRINTF(errstr, SIZEOF(errstr),
				"fwrite() : %s : Expected = %lld : Actual = %lld",
						(stdout == fp) ? "-STDOUT" : fname, extrlen, ret_size);
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8)
						ERR_SYSCALL, 5, LEN_AND_STR(errstr), CALLFROM, save_errno);
		}
	}
}
