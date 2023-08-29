/****************************************************************
 *								*
 * Copyright (c) 2001-2021 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef GVCMZ_NETERR_INCLUDED
#define GVCMZ_NETERR_INCLUDED

void gvcmz_neterr(gparam_list *err_plist);	/* separate include as routine is in deferred events;
						 * don't want to drag in all of gvcmz*.
						 */

#endif /* GVCMZ_NETERR_INCLUDED */
