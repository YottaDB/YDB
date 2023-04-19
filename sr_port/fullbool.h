/****************************************************************
 *								*
 * Copyright (c) 2010-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/
#ifndef FULLBOOL_H_INCLUDED
#define FULLBOOL_H_INCLUDED

enum gtm_bool_type
{
	GTM_BOOL = 0,		/* original GT.M short-circuit Boolean evaluation with naked maintenance */
	FULL_BOOL,		/* standard behavior - evaluate everything with a side effect */
	FULL_BOOL_WARN,		/* like FULL_BOOL but give compiler warnings when it makes a difference */
	EXT_BOOL		/* like FULL_BOOL, but also evaluate expressions with no side effects */
};

enum gtm_se_type
{
	OLD_SE = 0,		/* ignore side effect implications */
	STD_SE,			/* reorder argument processing for left-to-right side effects */
	SE_WARN			/* like STD but give compiler warnings when it makes a difference */
};

#define CONVERT_TO_SE(T)													\
MBSTART {	/* T is triple; this macro converts its opcode to the SE-equivalent */						\
	switch(T->opcode)	 												\
	{															\
	case OC_AND:														\
		T->opcode = OC_SAND;												\
		break;														\
	case OC_NAND:														\
		T->opcode = OC_SNAND;												\
		break;														\
	case OC_OR:														\
		T->opcode = OC_SOR;												\
		break;														\
	case OC_NOR:														\
		T->opcode = OC_SNOR;												\
		break;														\
	case OC_COBOOL:	/* WARNING - Only invoke this macro if you can guarantee bx_tail cleanup, or guard against COBOOLS */	\
		T->opcode = OC_SCOBOOL;												\
		break;														\
	default:														\
		break;														\
	}															\
} MBEND

#endif /* FULLBOOL_H_INCLUDED */
