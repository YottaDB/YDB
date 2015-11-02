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
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"

GBLREF unsigned char	jnl_ver;

void jnl_setver(void)
{
	char		*jnl_label = JNL_LABEL_TEXT;
	unsigned char	jnl_ver_lower, jnl_ver_higher;

	jnl_ver_lower = jnl_label[SIZEOF(JNL_LABEL_TEXT) - 2] - '0';
	assert('\012' > jnl_ver_lower); /* assert(10 > jnl_ver_lower); */
	jnl_ver_higher = jnl_label[SIZEOF(JNL_LABEL_TEXT) - 3] - '0';
	assert('\012' > jnl_ver_higher); /* assert(10 > jnl_ver_higher); */
	jnl_ver = jnl_ver_higher * 10 + jnl_ver_lower;
	assert(JNL_VER_THIS == jnl_ver);
	assert(JNL_VER_EARLIEST_REPL <= jnl_ver);
	return;
}
