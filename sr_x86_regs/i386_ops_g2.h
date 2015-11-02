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

/* Opcodes for group 2 (one-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(ROL,_,0)
I386_OP(ROR,_,1)
I386_OP(RCL,_,2)
I386_OP(RCR,_,3)
I386_OP(SHL,_,4)
I386_OP(SHR,_,5)
I386_OP(ILLEGAL_GROUP_2_OP,2,2)
I386_OP(SAR,_,7)
