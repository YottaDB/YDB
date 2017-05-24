/****************************************************************
 *                                                              *
 * Copyright (c) 2001-2016 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *                                                              *
 *    This source code contains the intellectual property       *
 *    of its copyright holder(s), and is made available         *
 *    under a license.  If you do not know the terms of         *
 *    the license, please stop and do not read further.         *
 *                                                              *
 ****************************************************************/

#ifndef GVN2GDS_included
#define GVN2GDS_included

#include "mdef.h"

#include <stdarg.h>
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"

unsigned char *gvn2gds(mval *gvn, gv_key *gvkey, int act);

/* Assumes that buff is of size MAX_ZWR_KEY_SZ. Reverses the operation done by gvn2gds */
unsigned char *gds2gvn(mval *gds, unsigned char *buff, int col);

#endif
