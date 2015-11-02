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

/* Opcodes for group 5 (one-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(INC,Ev,0)
I386_OP(DEC,Ev,1)
I386_OP(CALL,Ev,2)
I386_OP(CALL,Ep,3)
I386_OP(JMP,Ev,4)
I386_OP(JMP,Ep,5)
I386_OP(PUSH,Ev,6)
I386_OP(ILLEGAL_GROUP_5_OP,7,7)
