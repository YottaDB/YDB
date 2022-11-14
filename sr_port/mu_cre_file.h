/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2022 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef MU_CRE_FILE_INCLUDED
#define MU_CRE_FILE_INCLUDED

#define	CALLER_IS_MUPIP_CREATE_FALSE	FALSE
#define	CALLER_IS_MUPIP_CREATE_TRUE	TRUE

unsigned char mu_cre_file(boolean_t caller_is_mupip_create);

#endif /* MU_CRE_FILE_INCLUDED */
