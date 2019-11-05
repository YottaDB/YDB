/****************************************************************
 *								*
 * Copyright 2001, 2014 Fidelity Information Services, Inc	*
 *								*
 * Copyright (c) 2018-2019 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef OBJ_FILE_INCLUDED
#define OBJ_FILE_INCLUDED

#include <rtnhdr.h>	/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */
#include <obj_filesp.h>	/* see HDR_FILE_INCLUDE_SYNTAX comment in mdef.h for why <> syntax is needed */

/* YottaDB objects define 2 local symbols in the symbol table. The Elf64_shdr->sh_info field we fill in
 * during object creation is documented to be ONE MORE than the highest local symbol table index used.
 * In our case, the highest local symbol index (0 based) is 1 so we set the sh_info field to 2. This
 * define provides the value to use for sh_info on all platforms.
 */
#define YDB_MAX_LCLSYM_INDX_P1 (1 + 1)

#define OUTPUT_SYMBOL_SIZE (SIZEOF(int4) + sym_table_size)
#define PADCHARS	"PADDING PADDING"
#define RENAME_TMP_OBJECT_FILE(FNAME) rename_tmp_object_file(FNAME)
#ifndef SECTION_ALIGN_BOUNDARY
#if defined(GTM64)
#	define SECTION_ALIGN_BOUNDARY  16
#else
#       define SECTION_ALIGN_BOUNDARY  8
#endif /* GTM64 */
#endif /* SECTION_ALIGN_BOUNDARY */
#define OBJECT_SIZE_ALIGNMENT 16
#ifdef DEBUG
#define MAX_CODE_COUNT 10000
/* This structure holds the size of the code generated for every triple */
struct inst_count
{
	int size;
	int sav_in;
};
#endif /* DEBUG */

/* Prototypes */
#ifdef UNIX
int mk_tmp_object_file(const unsigned char *object_fname, int object_fname_len);
void rename_tmp_object_file(const unsigned char *object_fname);
void init_object_file_name(void);
void finish_object_file(void);
#endif
void emit_immed(char *source, uint4 size);
void emit_literals(void);
void emit_linkages(void);
int literal_offset(UINTPTR_T offset);
int4 find_linkage(mstr* name);
void drop_object_file(void);
void create_object_file(rhdtyp *rhead);
void obj_init(void);
VMS_ONLY(void close_object_file(rhdtyp *rhead);)
DEBUG_ONLY(int output_symbol_size(void);)
#endif /* OBJ_FILE_INCLUDED */
