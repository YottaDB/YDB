/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OBJ_FILE_INCLUDED
#define OBJ_FILE_INCLUDED

#include "obj_filesp.h"

void emit_immed(char *source, uint4 size);
void emit_literals(void);
void emit_linkages(void);
int4 literal_offset(int4 offset);
int4 find_linkage(mstr* name);
void drop_object_file(void);
void close_object_file(void);
void create_object_file(rhdtyp *rhead);
void obj_init(void);
#endif
