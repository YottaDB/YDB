/****************************************************************
 *								*
 *	Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OBJ_FILESP_INCLUDED
#define OBJ_FILESP_INCLUDED

void emit_addr(int4 refaddr, int4 offset, int4 *result);
void emit_reference(uint4 refaddr, mstr *name, uint4 *result);
struct sym_table *define_symbol(unsigned char psect, mstr *name, int4 value);
void emit_pidr(int4 refoffset, int4 data_offset, int4 *result);
void buff_emit(void);
void set_psect(unsigned char psect,unsigned char offset);
void resolve_sym(void);
void output_relocation(void);
void output_symbol(void);

#endif

