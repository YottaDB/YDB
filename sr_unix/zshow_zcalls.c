/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
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
#include <stdarg.h>
#include <errno.h>
#include "gtm_stdlib.h"
#include "copy.h"
#include "lv_val.h"		/* needed for "fgncal.h" */
#include "fgncal.h"
#include "gtmxc_types.h"
#include "mvalconv.h"
#include "util.h"
#include "zshow.h"

#define	DO_ONE_ITEM(OUTPUT, BUFF, NBYTES)						\
{											\
	mstr	line;									\
											\
	if (NBYTES >= SIZEOF(BUFF))							\
		NBYTES = SIZEOF(BUFF); /* Output from SNPRINTF was truncated. */	\
	line.len = NBYTES;								\
	line.addr = &BUFF[0];								\
	OUTPUT->flush = TRUE;								\
	zshow_output(OUTPUT, &line);							\
}

void zshow_zcalls(zshow_out *output)
{
	struct 	extcall_package_list	*package_ptr;
	struct 	extcall_entry_list	*entry_ptr;
	int	nbytes, totalbytes;
	char	buff[OUT_BUFF_SIZE];
	int 	len;

	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	/* Go through loaded packages */
	for (package_ptr = TREF(extcall_package_root); package_ptr; package_ptr = package_ptr->next_package)
	{
		/* Go through the package's entry points */
		for (entry_ptr = package_ptr->first_entry; entry_ptr; entry_ptr = entry_ptr->next_entry)
		{
			nbytes = SNPRINTF(buff, SIZEOF(buff), "%s.%s",package_ptr->package_name.addr,
				entry_ptr->entry_name.addr);
			DO_ONE_ITEM(output, buff, nbytes);
		}
	}
	return;
}
