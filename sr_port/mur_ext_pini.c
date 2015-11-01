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

void	mur_extract_pini(jnl_record *rec)
{
	int	extract_len = 0;

	if (!mur_options.detail)
	{
		EXT2BYTES(&muext_code[MUEXT_PINI][0]);
		extract_len = extract_process_vector(&rec->val.jrec_pini.process_vector[CURR_JPV], extract_len);
	} else
	{
		extract_len = strlen(mur_extract_buff);
		strcpy(&mur_extract_buff[extract_len], "PINI   \\");
		extract_len = strlen(mur_extract_buff);
		extract_len = extract_process_vector((jnl_process_vector *)&rec->val.jrec_pini.process_vector[CURR_JPV],
														extract_len);
		extract_len = extract_process_vector((jnl_process_vector *)&rec->val.jrec_pini.process_vector[ORIG_JPV],
														extract_len);
		extract_len = extract_process_vector((jnl_process_vector *)&rec->val.jrec_pini.process_vector[SRVR_JPV],
														extract_len);
	}
	jnlext_write(mur_extract_buff, extract_len);
}

int extract_process_vector(jnl_process_vector *pv, int extract_len)
{
	int			actual;
	char			*ptr;
	jnl_proc_time		*ref_time;

	ref_time = &pv->jpv_time;
	EXTTIME(ref_time);
	EXTINT(pv->jpv_pid);
	EXTTXTVMS(pv->jpv_prcnam, JPV_LEN_PRCNAM);
	EXTTXT(pv->jpv_node, JPV_LEN_NODE);
	EXTTXT(pv->jpv_user, JPV_LEN_USER);
	EXTINTVMS(pv->jpv_mode);
	EXTTXT(pv->jpv_terminal, JPV_LEN_TERMINAL);
	EXTTIMEVMS(pv->jpv_login_time);
	EXTINTVMS(pv->jpv_image_count);
	return extract_len;
}
