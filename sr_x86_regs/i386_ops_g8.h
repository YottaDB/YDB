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

/* Opcodes for group 8 (two-byte) determined by bits 5,4,3 of ModR/M byte: */
I386_OP(ILLEGAL_GROUP_8_OP,0,0)
I386_OP(ILLEGAL_GROUP_8_OP,1,1)
I386_OP(ILLEGAL_GROUP_8_OP,2,2)
I386_OP(ILLEGAL_GROUP_8_OP,3,3)
I386_OP(BT,_,4)
I386_OP(BTS,_,5)
I386_OP(BTR,_,6)
I386_OP(BTC,_,7)
