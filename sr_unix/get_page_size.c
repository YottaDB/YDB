/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2024 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"
#include "gtm_unistd.h"
#include "get_page_size.h"
#include "gtm_sizeof.h"
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtmio.h"

GBLDEF int4	gtm_os_page_size;
GBLDEF long	gtm_os_hugepage_size = -1;
GBLREF bool	hugetlb_shm_enabled;

void get_page_size(void)
{
	gtm_os_page_size = getpagesize();

	return;
}

void get_hugepage_size(void)
{
	FILE	*info;
	int	hps, res;
	char	line[1024];
	char	*fgets_res;

	gtm_os_hugepage_size = gtm_os_page_size; /* Default to regular page size */
	if (hugetlb_shm_enabled)
	{
		Fopen(info, "/proc/meminfo", "r");
		if (NULL != info)
		{
			FEOF(info, res);
			while (!res)
			{
				res = FSCANF(info, "Hugepagesize: %d kB", &hps);
				if (EOF == res || ferror(info))
					break;
				else if (1 == res && 0 < hps)
				{
					gtm_os_hugepage_size = (1024 * (long)hps);
					break;
				}
				else if (0 != res)
					break;
				FGETS(line, SIZEOF(line), info, fgets_res); /* Consume the line */
				FEOF(info, res);
			}
			FCLOSE(info, res);
		}
	}

	return;
}
