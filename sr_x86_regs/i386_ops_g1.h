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

/* Opcodes for group 1 (one-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(ADD,_,0)
I386_OP(OR,_,1)
I386_OP(ADC,_,2)
I386_OP(SBB,_,3)
I386_OP(AND,_,4)
I386_OP(SUB,_,5)
I386_OP(XOR,_,6)
I386_OP(CMP,_,7)
