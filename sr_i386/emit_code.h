/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef EMIT_CODE_INCLUDED
#define EMIT_CODE_INCLUDED

void trip_gen(triple *ct);
short *emit_vax_inst(short *inst, oprtype **fst_opr, oprtype **lst_opr);
void emit_xfer(short xfer);
void emit_base_offset (short reg_opcode, short base_reg, int4 offset);

#endif
