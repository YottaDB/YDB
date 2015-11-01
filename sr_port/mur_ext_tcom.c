/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include "copy.h"
#include "mur_ext_set.h"

GBLREF	char		*mur_extract_buff;
GBLREF	mur_opt_struct	mur_options;
GBLREF	char		muext_code[][2];

GBLREF	boolean_t	tstarted;



/* This routine formats and outputs journal extract records
   corresponding to M TCOMMIT and ZTCOMMIT commands */

void	mur_extract_tcom(
			 jnl_record	*rec,
			 uint4		pid)
{
	int	actual, extract_len = 0;
	char	*ptr;

	if (REF_CHAR(&rec->jrec_type) == JRT_ZTCOM)
	{
		EXT2BYTES(&muext_code[MUEXT_ZTCOMMIT][0]);
	} else
	{
		EXT2BYTES(&muext_code[MUEXT_TCOMMIT][0]);
	}

	if (REF_CHAR(&rec->jrec_type) == JRT_ZTCOM)
	{
		EXTTIME(rec->val.jrec_ztcom.ts_short_time);
		EXTTIME(rec->val.jrec_ztcom.tc_short_time);
	} else
		EXTTIME(rec->val.jrec_tcom.tc_short_time);

	EXTINT(pid);

	EXTQW(rec->val.jrec_tcom.jnl_seqno);
	EXTINT(rec->val.jrec_tcom.tn);

	EXTINT(rec->val.jrec_tcom.participants);

	jnlext_write(mur_extract_buff, extract_len);
}

void	detailed_extract_tcom(
			 jnl_record	*rec,
			 uint4		pid)
{
	int	actual, extract_len;
	char	*ptr;

	if (TRUE == tstarted)
		return;

	if (JRT_TCOM == rec->jrec_type)
	{
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "TCOMMIT\\");
		extract_len = strlen(mur_extract_buff);
		EXTTIME(rec->val.jrec_tcom.tc_short_time);
		EXTTIME(rec->val.jrec_tcom.tc_recov_short_time);
	} else
	{
		extract_len = strlen(mur_extract_buff);
		strcpy(mur_extract_buff + extract_len, "ZTCOMMIT\\");
		extract_len = strlen(mur_extract_buff);
		EXTTIME(rec->val.jrec_ztcom.ts_short_time);
		EXTTIME(rec->val.jrec_ztcom.ts_recov_short_time);
		EXTTIME(rec->val.jrec_ztcom.tc_short_time);
		EXTTIME(rec->val.jrec_ztcom.tc_recov_short_time);
	}
	EXTINT(pid);
	EXTQW(rec->val.jrec_tcom.token);

	EXTQW(rec->val.jrec_tcom.jnl_seqno);
	EXTINT(rec->val.jrec_tcom.tn);
	EXTINT(rec->val.jrec_tcom.participants);

	jnlext_write(mur_extract_buff, extract_len);
}
