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

#ifndef OBJ_FILESP_INCLUDED
#define OBJ_FILESP_INCLUDED

void emit_addr(int4 refaddr, int4 offset, int4 *result);
void emit_reference(uint4 refaddr, mstr *name, uint4 *result);
struct sym_table *define_symbol(unsigned char psect, mstr name, int4 value);

#endif

