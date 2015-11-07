/****************************************************************
 *								*
 *	Copyright 2001, 2004 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OBJ_FILESP_INCLUDED
#define OBJ_FILESP_INCLUDED

void emit_pidr(int4 offset, unsigned char psect);
void emit_reference(uint4 refaddr, mstr *name, uint4 *result);
struct sym_table *define_symbol(int4 psect, mstr *name, int4 value);

/* Prefix of the psect name generated for every routine table entry (used in obj_file.c */
#define RNAMB_PREF		"GTM$R"
#define RNAMB_PREF_LEN		STR_LIT_LEN(RNAMB_PREF)

/* First significant characters of the routine name on which the table is already sorted by the VMS linker */
#define RNAME_SORTED_LEN	(EGPS$S_NAME - RNAMB_PREF_LEN)	/* EGPS$S_NAME defined in objlangdefs.h */

#define RNAME_ALL_Z		"zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz"

#endif /* OBJ_FILESP_INCLUDED */
