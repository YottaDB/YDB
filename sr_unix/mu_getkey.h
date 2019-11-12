/****************************************************************
 *								*
 * Copyright (c) 2019 Fidelity National Information		*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef MUPGETKEY_H
#define MUPGETKEY_H
/*  requires gdsroot.h */

#define MUKEY_FALSE          0 /* -subscript was NOT specified as part of mupip integ */
#define MUKEY_TRUE           1 /* -subscript was specified as part of mupip integ */
#define MUKEY_NULLSUBS       2 /* -subscript was specified as part of mupip integ AND there was at least one null subscript */

#define DUMMY_GLOBAL_VARIABLE           "%D%DUMMY_VARIABLE"
#define DUMMY_GLOBAL_VARIABLE_LEN       SIZEOF(DUMMY_GLOBAL_VARIABLE)

GBLDEF gv_key *mu_start_key;
GBLDEF gv_key *mu_end_key;
GBLDEF int mu_start_keyend;
GBLDEF int mu_end_keyend;

int mu_getkey(unsigned char *key_buff, int keylen);
#endif

