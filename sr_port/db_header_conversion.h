/****************************************************************
 *								*
 * Copyright (c) 2020 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef DB_HEADER_CONVERSION_DEFINED
#define DB_HEADER_CONVERSION_DEFINED

#include "v6_gdsfhead.h"
#include "gdsfhead.h"

void db_header_upconv(sgmnt_data_ptr_t v7);
void db_header_dwnconv(sgmnt_data_ptr_t v6);

#endif
