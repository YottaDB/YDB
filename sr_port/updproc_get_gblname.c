/****************************************************************
 *								*
 *	Copyright 2005, 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#include "mdef.h"

#ifdef VMS
#include <ssdef.h>
#include <descrip.h> /* Required for gtmsource.h */
#endif

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "jnl.h"
#include "jnl_typedef.h"
#include "gtmrecv.h"
#include "read_db_files_from_gld.h" /* for updproc.h */
#include "updproc.h"
#include "updproc_get_gblname.h"
#include "min_max.h"

/*
 * This rourine validates the key from journal record copying to memory pointed updproc_get_gblname
 * The mname is an mstr for the global name (to be used for gv_bind_name) is set here.
 */

enum upd_bad_trans_type updproc_get_gblname(char *src_ptr, int key_len, char *gv_mname, mstr *mname)
{
	char			*dest_ptr;
	int			cplen;

	cplen = MIN(MAX_MIDENT_LEN + 1, key_len);	/* +1 to consider null */
	dest_ptr = (char *)gv_mname;
	do
	{
		cplen--;
		if (0 == (*dest_ptr++ = *src_ptr++))
			break;
	} while (cplen);
	if (0 != (*(dest_ptr - 1)) || (0 >= dest_ptr - 1 - gv_mname))
		return upd_bad_mname_size;
	mname->addr = (char *)gv_mname;
	mname->len = INTCAST(dest_ptr - 1 - (char *)gv_mname);
	return upd_good_record;
}
