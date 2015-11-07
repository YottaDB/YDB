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

#ifndef OBJ_FILE_INCLUDED
#define OBJ_FILE_INCLUDED

#include <obj_filesp.h>

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
int mk_tmp_object_file(const char *object_fname, int object_fname_len);
void rename_tmp_object_file(const char *object_fname);
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
