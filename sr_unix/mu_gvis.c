/****************************************************************
 *								*
 *	Copyright 2001, 2006 Fidelity Information Services, Inc	*
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
#include "util.h"
#include "format_targ_key.h"
#include "mu_gvis.h"

GBLREF gv_key	*gv_currkey;

void mu_gvis(void )
{
	char key_buff[MAX_ZWR_KEY_SZ], *key_end;

	if (gv_currkey->end)
	{
		if ((key_end = (char*)format_targ_key((uchar_ptr_t)&key_buff[0], MAX_ZWR_KEY_SZ, gv_currkey, TRUE)) == 0)
			key_end = &key_buff[MAX_ZWR_KEY_SZ - 1];
	} else
		key_end = &key_buff[0];
	util_out_print("!_!_Global variable : !AD", TRUE, key_end - &key_buff[0], key_buff);
	return;
}
