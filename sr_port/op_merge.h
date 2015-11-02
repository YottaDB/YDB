/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* ----------
 * op_merge.h
 *-----------
 * Following structure save results of parsing the left or, right hand side of MERGE.
 * They can be either global-global, local-local, local-global, global-local
 */
#ifndef OP_MERGE_DEFINED

typedef struct merge_glvn_struct_type
{
	gvname_info 	*gblp[2];
	lv_val		*lclp[2];
} merge_glvn_struct;
typedef merge_glvn_struct *merge_glvn_ptr;

/* Function Prototypes */
void 	op_merge(void);
void 	op_merge_arg(int m_opr_type, lv_val *lvp);
boolean_t 	merge_desc_check(void); /* returns 1 if ok to merge, 0 if merge self, error if descendants */

#define OP_MERGE_DEFINED
#endif
