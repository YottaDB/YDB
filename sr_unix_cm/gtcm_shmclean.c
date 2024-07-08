/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright (c) 2017-2018 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
||||||| parent of 07a11a26bb (GT.M V7.1-004)
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
=======
 * Copyright (c) 2001-2024 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
>>>>>>> 07a11a26bb (GT.M V7.1-004)
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "cli.h"
<<<<<<< HEAD
||||||| parent of 07a11a26bb (GT.M V7.1-004)
#include "gtm_threadgbl_init.h"
=======
#include "gtm_threadgbl_init.h"
#include "hugetlbfs_overrides.h"
>>>>>>> 07a11a26bb (GT.M V7.1-004)

int main(int argc, char **argv, char **envp)
{
	return dlopen_libyottadb(argc, argv, envp, "gtcm_shmclean_main");
}
