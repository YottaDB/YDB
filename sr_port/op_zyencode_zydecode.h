/****************************************************************
 *								*
 * Copyright (c) 2025 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ----------
 * op_zyencode_zydecode.h
 *-----------
 */
#ifndef OP_ZYENCODE_ZYDECODE_DEFINED

#include "tp.h"
#include "gvname_info.h"

/* The following structure saves the results of parsing the left and right hand side of ZYENCODE.
 * They can be either global-global, local-local, local-global, or global-local
 */
typedef struct zyencode_glvn_struct_type
{
	gvname_info 	*gblp[2];
	lv_val		*lclp[2];
} zyencode_glvn_struct;
typedef zyencode_glvn_struct *zyencode_glvn_ptr;

/* The following structure saves the results of parsing the left and right hand side of ZYDECODE.
 * They can be either global-global, local-local, local-global, or global-local
 */
typedef struct zydecode_glvn_struct_type
{
	gvname_info 	*gblp[2];
	lv_val		*lclp[2];
} zydecode_glvn_struct;
typedef zydecode_glvn_struct *zydecode_glvn_ptr;

/* Function Prototypes */
void	op_zyencode(void);
void	op_zydecode(void);
void	op_zyencode_arg(int e_opr_type, lv_val *lvp);
void	op_zydecode_arg(int d_opr_type, lv_val *lvp);
void 	zyencode_zydecode_desc_check(int desc_error); /* Returns if ok to continue, or error if descendants */

#define OP_ZYENCODE_ZYDECODE_DEFINED
#endif
