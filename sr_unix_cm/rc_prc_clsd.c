/****************************************************************
 *								*
<<<<<<< HEAD
 * Copyright 2001 Sanchez Computer Associates, Inc.		*
 *								*
 * Copyright (c) 2017 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
||||||| parent of 07a11a26bb (GT.M V7.1-004)
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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
#include "gdsroot.h"
#include "gdsblk.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "rc.h"
#include "copy.h"

int
rc_prc_clsd(rc_q_hdr *qhdr)
{
<<<<<<< HEAD

	ASSERT_IS_LIBGTCM;
||||||| parent of 07a11a26bb (GT.M V7.1-004)

=======
>>>>>>> 07a11a26bb (GT.M V7.1-004)
    qhdr->a.erc.value = RC_SUCCESS;

    return 0;
}
