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

/* 32-bit register definitions: */
REGDEF(EAX,0),
REGDEF(ECX,1),
REGDEF(EDX,2),
REGDEF(EBX,3),

REGDEF(ESP,4),
REGDEF(SIB_FOLLOWS,4) = I386_REG_ESP,
REGDEF(NO_INDEX,4) = I386_REG_ESP,
REGDEF(EBP,5),
REGDEF(disp32_NO_BASE,5) = I386_REG_EBP,
REGDEF(disp32_FROM_RIP,5) = I386_REG_EBP,
REGDEF(ESI,6),
REGDEF(EDI,7),

