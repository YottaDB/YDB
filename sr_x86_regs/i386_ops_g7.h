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

/* Opcodes for group 7 (two-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(SGDT,Ms,0)
I386_OP(SIDT,Ms,1)
I386_OP(LGDT,Ms,2)
I386_OP(LIDT,Ms,3)
I386_OP(SMSW,Ew,4)
I386_OP(ILLEGAL_GROUP_7_OP,5,5)
I386_OP(LMSW,Ew,6)
I386_OP(ILLEGAL_GROUP_7_OP,7,7)
