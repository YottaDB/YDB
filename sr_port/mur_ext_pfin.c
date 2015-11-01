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
#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "muprec.h"
#include "mur_ext_set.h"

GBLREF	char		*mur_extract_buff;
GBLREF	mur_opt_struct	mur_options;
GBLREF	char		muext_code[][2];



void	mur_extract_pfin(jnl_record *rec)
{
	int			actual, extract_len = 0;
	char			*ptr;
	jnl_process_vector	*pv = &rec->val.jrec_pfin.process_vector;
	jnl_proc_time		*ref_time = &pv->jpv_time;


	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_PFIN][0]);
	}
	else
	{
		extract_len = strlen(mur_extract_buff);
		strcpy(&mur_extract_buff[extract_len], "PFIN   \\");
		extract_len = strlen(mur_extract_buff);
	}

	EXTTIME(ref_time);

	EXTINT(pv->jpv_pid);

	EXTTXTVMS(pv->jpv_prcnam, JPV_LEN_PRCNAM);

	EXTTXT(pv->jpv_node, JPV_LEN_NODE);

	EXTTXT(pv->jpv_user, JPV_LEN_USER);

	EXTINTVMS(pv->jpv_mode);

	EXTTXT(pv->jpv_terminal, JPV_LEN_TERMINAL);

	EXTTIMEVMS(pv->jpv_login_time);

	EXTINTVMS(pv->jpv_image_count);

	EXTINT(rec->val.jrec_pfin.tn);

	jnlext_write(mur_extract_buff, extract_len);
}
