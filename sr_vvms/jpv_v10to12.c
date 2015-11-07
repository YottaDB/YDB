/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jpv_v10to12.h"

void jpv_v10to12(char *old_jpv_ptr, jnl_process_vector *new_jpv)
{
	jnl_process_vector	temp_jpv;
	v10_jnl_process_vector	*old_jpv;

	memset(&temp_jpv, 0, SIZEOF(jnl_process_vector));
	old_jpv = (v10_jnl_process_vector *) old_jpv_ptr;
	temp_jpv.jpv_pid = old_jpv->jpv_pid;
	temp_jpv.jpv_time = old_jpv->jpv_time;
	temp_jpv.jpv_login_time = old_jpv->jpv_login_time;
	temp_jpv.jpv_image_count = old_jpv->jpv_image_count;
	temp_jpv.jpv_mode = old_jpv->jpv_mode;
	memcpy(&temp_jpv.jpv_node, old_jpv->jpv_node, V10_JPV_LEN_NODE);
	memcpy(&temp_jpv.jpv_user, old_jpv->jpv_user, V10_JPV_LEN_USER);
	memcpy(&temp_jpv.jpv_prcnam, old_jpv->jpv_prcnam, V10_JPV_LEN_PRCNAM);
	memcpy(&temp_jpv.jpv_terminal, old_jpv->jpv_terminal, V10_JPV_LEN_TERMINAL);
	memcpy(new_jpv, &temp_jpv, SIZEOF(jnl_process_vector));
}
