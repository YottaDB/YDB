/****************************************************************
 *								*
 * Copyright (c) 2018-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_post_startup_check_init.h"
#include "gtm_icu_api.h"
#ifdef __linux__
#include "hugetlbfs_overrides.h"
#include "get_page_size.h"
#endif

/* This function exists to perform all initialization that can only be done
 * after $gtm_dist has been validated by gtm_startup_chk. The reason for this
 * is two fold:
 *	1. err_init() is performed before this point so that a minimal
 *	   environment exists to report an error. This is a requirement for
 *	   gtm_startup_chk.
 *	2. Initializations require a verified $gtm_dist to enforce
 *	   restrictions. AKA restrictions will be initialized now.
 */

void gtm_post_startup_check_init(void)
{
#	ifdef HUGETLB_SUPPORTED
	get_page_size();
	get_hugepage_size();
#	endif
	GTM_ICU_INIT_IF_NEEDED;
}
