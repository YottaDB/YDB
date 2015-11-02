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

#include "gtm_string.h"

#include "cli.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "murest.h"
#include "mupip_exit.h"

GBLREF inc_list_struct in_files;

static readonly char name[] = "INPUT_FILE";
void murgetlst(void)
{
	char		*c1, *c2, buff[MAX_LINE];
	unsigned short	len;
	inc_list_struct	*ptr;
	error_def(ERR_MUNODBNAME);

	len = SIZEOF(buff);
	if (!cli_get_str(name,buff,&len))
		mupip_exit(ERR_MUNODBNAME);

	ptr = &in_files;
	for (c1 = c2 = buff; ; )
	{	for ( ; *c2 && (*c2 != ',') ; c2++)
			;
		ptr->next = (inc_list_struct*)malloc(SIZEOF(inc_list_struct));
		ptr = ptr->next;
		ptr->next = 0;
		ptr->input_file.len = INTCAST(c2 - c1);
		ptr->input_file.addr = (char *)malloc(c2 - c1 + 1);
		memcpy(ptr->input_file.addr, c1, c2 - c1);
		*(char*)(ptr->input_file.addr + (c2 - c1)) = '\0';
		if (!*c2)
			break;
		else
			c1 = ++c2;
	}
	return;
}
