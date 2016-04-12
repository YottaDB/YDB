/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include <errno.h>
#include "gtm_string.h"
#include "gtm_stdio.h"
#include "gtm_fcntl.h"
#include "gtm_unistd.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "mlkdef.h"
#include "filestruct.h"
#include "gbldirnam.h"
#include "hashtab_mname.h"
#include "hashtab.h"

static gdr_name	*gdr_name_head;

gd_addr *create_dummy_gbldir(void)
{
	header_struct	*header;
	gd_addr		*addr;
	gdr_name	*name;
	gd_region	*region;
	gd_region	*region_top;
	gd_segment	*segment;
	uint4		size;

	DUMMY_GLD_INIT(header, addr, region, segment, size, RELATIVE_OFFSET_FALSE);
		/* the above macro invocation initializes "header", "addr", "region", "segment" and "size" */
	addr->id = (gd_id *)malloc(SIZEOF(gd_id));
	memset(addr->id, 0, SIZEOF(gd_id));

	addr->tab_ptr = (hash_table_mname *)malloc(SIZEOF(hash_table_mname));
	init_hashtab_mname((hash_table_mname *)addr->tab_ptr, 0, HASHTAB_NO_COMPACT, HASHTAB_NO_SPARE_TABLE );

	name = (gdr_name *)malloc(SIZEOF(gdr_name));
	MALLOC_CPY_LIT(name->name.addr, "DUMMY.GLD");
	if (gdr_name_head)
		name->link = (gdr_name *)gdr_name_head;
	else
		name->link = 0;
	gdr_name_head = name;
	gdr_name_head->gd_ptr = addr;
	return addr;
}
