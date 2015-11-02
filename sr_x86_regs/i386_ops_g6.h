/****************************************************************
 *								*
 *	Copyright 2007 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Opcodes for group 6 (two-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(SLDT,Ew,0)
I386_OP(STR,Ew,1)
I386_OP(LLDT,Ew,2)
I386_OP(LTR,Ew,3)
I386_OP(VERR,Ew,4)
I386_OP(VERW,Ew,5)
I386_OP(ILLEGAL_GROUP_6_OP,6,6)
I386_OP(ILLEGAL_GROUP_6_OP,7,7)
