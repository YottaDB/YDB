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

/* Opcodes for group 3 (one-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(TEST,Ib_Iv,0)
I386_OP(ILLEGAL_GROUP_3_OP,1,1)
I386_OP(NOT,_,2)
I386_OP(NEG,_,3)
I386_OP(MUL,AL_eAX,4)
I386_OP(IMUL,AL_eAX,5)
I386_OP(DIV,AL_eAX,6)
I386_OP(IDIV,AL_eAX,7)
