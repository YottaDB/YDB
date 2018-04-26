/****************************************************************
 *								*
 * Copyright (c) 2001-2015 Fidelity National Information 	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018 YottaDB LLC. and/or its subsidiaries.	*
 * All rights reserved.						*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include "gtm_stdio.h"
#include <errno.h>
#include "gtm_stat.h"
#include <sys/types.h>
#include "gtm_stdlib.h"
#include "gtm_unistd.h"

#include "compiler.h"
#include "obj_gen.h"
#include <rtnhdr.h>
#include "cmd_qlf.h"
#include "cgp.h"
#include "gtmio.h"
#include "eintr_wrappers.h"
#include "mmemory.h"
#include "obj_file.h"
#include "alloc_reg.h"
#include "jmp_opto.h"
#include "mlabel2xtern.h"
#include "cg_var.h"
#include "gtm_string.h"
#include "objlabel.h"
#include "stringpool.h"
#include "min_max.h"
#include "rtn_src_chksum.h"
#include "mmrhash.h"
#include "arlinkdbg.h"
#include "incr_link.h"
#include "have_crit.h"

GBLDEF uint4 			lits_text_size, lits_mval_size;

GBLREF int4			curr_addr, code_size;
GBLREF char			cg_phase;	/* code generation phase */
GBLREF char			cg_phase_last;	/* previous code generation phase */
GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;
GBLREF int4			mvmax, mlmax, mlitmax, sa_temps[], sa_temps_offset[];
GBLREF mlabel 			*mlabtab;
GBLREF mline 			mline_root;
GBLREF mvar 			*mvartab;
GBLREF mident			module_name, int_module_name;
GBLREF int4			gtm_object_size;
GBLREF int4			sym_table_size;
GBLREF int4			linkage_size;
GBLREF uint4			lnkrel_cnt;	/* number of entries in linkage Psect to relocate */
GBLREF spdesc			stringpool;
GBLREF short			object_name_len;
GBLREF char			object_file_name[];
GBLREF int			object_file_des;

/* The sections of the internal GT.M object (sans native object wrapper) are grouped
 * according to their type (R/O-retain, R/O-release, R/W-retain, R/W-release). The
 * "retain"/"release" refers to whether the sections in that segment are retained if the
 * module is replaced by an explicit ZLINK.
 *
 * The GT.M object layout (in the native .text section) is as follows:
 *
 *	+---------------+
 *	|     rhead	| > - R/O - retain
 *	+---------------+                                Alternative layout if compiled with GTM_DYNAMIC_LITERALS:
 *	|   generated	| \                              \
 *	|     code	|  \                              \
 *	+ - - - - - - - +   |                              |
 *	| line num tbl	|   |- R/O releasable              |
 *	+ - - - - - - - +   |                              |- R/O releasable
 *	| lit text pool	|   |                              |
 *	+---------------+  /                               |
 *	| lkg name tbl 	| /              		   |
 *	+---------------+                                 /
 *	| lit mval tbl 	| \                              /
 *	+---------------+  |- R/W releasable
 *	| variable tbl	| /                              > - R/W releasable
 *	+ - - - - - - - +
 *	|   label tbl	| > - R/W retain
 *	+---------------+
 *	| relocations	| > - relocations for external syms (not kept after link)
 *	+---------------+
 *	|  symbol tbl 	| > - external symbol table (not kept after link)
 *	+---------------+
 *
 * Note in addition to the above layout, a "linkage section" is allocated at run time and is
 * also releasable. The linkage table and the linkage name table are co-indexed and provide the
 * routine or label names for the given entry. The linkage name table consists of unrelocated mstrs
 * as they are only ever used once so need not be resolved.
 *
 * If GTM_DYNAMIC_LITERALS is enabled, the literal mval table becomes part of the R/O-release section.
 * In the case of shared libraries, this spares each process from having to take a malloced copy the lit mval table
 * at link time (sr_unix/incr_link.c). This memory saving optimization is only available on USHBIN-supported platforms
 */

error_def(ERR_OBJFILERR);
error_def(ERR_SYSCALL);
error_def(ERR_TEXT);

STATICFNDCL void cg_lab (mlabel *mlbl, char *do_emit);

void obj_code (uint4 src_lines, void *checksum_ctx)
{
	int		i, status;
	uint4		lits_pad_size, object_pad_size, lnr_pad_size, lnkname_pad_size;
	int4		offset;
	int4		old_code_size;
	rhdtyp		rhead;
	mline		*mlx, *mly;
	gtm_uint16	objhash;
	var_tabent	*vptr;
	intrpt_state_t	prev_intrpt_state;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!run_time);
	obj_init();
	/* Define the routine name global symbol */
	define_symbol(GTM_MODULE_DEF_PSECT, (mstr *)&int_module_name);
	memset(&rhead, 0, SIZEOF(rhead));
	alloc_reg();
	jmp_opto();
	/* Note that this initial setting of curr_addr is historical in that the routine header was
	 * contiguous with the code. This is no longer true and this address would be more correct
	 * if it were set to zero however if that is done and the M code contains a branch or recursive
	 * DO to the top of this routine, that branch will trigger an assert failure in emit_code
	 * because a branch to "location 0" has long-jump implications. Keeping the initial offset
	 * non-zero avoids this problem. We had to add PTEXT_OFFSET to curr_addr to get code_size
	 * after the CGP_APPROX_ADDR phase anyway. Note that in using this offset, the rtaddr field
	 * of the triples has a PTEXT_OFFSET origin so this will need to be accounted for when the
	 * various tables below are generated using the rtaddr field. SE 10/2002
	 */
	curr_addr = PTEXT_OFFSET;
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	comp_lits(&rhead);
	old_code_size = code_size;
	shrink_trips();
	if ((cmd_qlf.qlf & CQ_MACHINE_CODE))
	{
		cg_phase = CGP_ASSEMBLY;
		code_gen();
	}
	if (!(cmd_qlf.qlf & CQ_OBJECT))
		return;
	/* Executable code offset setup */
	rhead.ptext_adr = (unsigned char *)PTEXT_OFFSET;
	rhead.ptext_end_adr = (unsigned char *)(size_t)code_size;
	/* Line number table offset setup */
	rhead.lnrtab_adr = (lnr_tabent *)rhead.ptext_end_adr;
	rhead.lnrtab_len = src_lines;
	code_size += src_lines * SIZEOF(lnr_tabent);
	lnr_pad_size = PADLEN(code_size, SECTION_ALIGN_BOUNDARY);
	code_size += lnr_pad_size;
	/* Literal text section offset setup */
	rhead.literal_text_adr = (unsigned char *)(size_t)code_size;
	rhead.literal_text_len = lits_text_size;
	code_size += lits_text_size;
	assert(0 == PADLEN(code_size, NATIVE_WSIZE));
	/* Literal extern (linkage) names section (same # of entries as linkage table) */
	rhead.linkage_names = (mstr *)(size_t)code_size;
	code_size += ((linkage_size / SIZEOF(lnk_tabent)) * SIZEOF(mstr));
	lnkname_pad_size = PADLEN(code_size, NATIVE_WSIZE);
	code_size += lnkname_pad_size;
	/* Literal mval section offset setup */
	rhead.literal_adr = (mval *)(size_t)code_size;
	rhead.literal_len = lits_mval_size / SIZEOF(mval);
	code_size += lits_mval_size;
	/* Padding so variable table starts on proper boundary */
	lits_pad_size = PADLEN(code_size, SECTION_ALIGN_BOUNDARY);
	code_size += lits_pad_size;
	/* Variable table offset setup */
	rhead.vartab_adr = (var_tabent *)(size_t)code_size;
	rhead.vartab_len = mvmax;
	code_size += mvmax * SIZEOF(var_tabent);
	/* Linkage section setup. No table in object but need its length. */
	rhead.linkage_adr = NULL;
	rhead.linkage_len = linkage_size / SIZEOF(lnk_tabent);
	/* Label table offset setup */
	rhead.labtab_adr = (lab_tabent *)(size_t)code_size;
	rhead.labtab_len = mlmax;
	code_size += mlmax * SIZEOF(lab_tabent);
	if (mlabtab)
		walktree((mvar *)mlabtab, cg_lab, NULL);
	/* Prior to calculating reloc and symbol size, run the linkage chains to fill out
	 * the reloc table and symbol table so we get the same size we will create later.
	 */
	comp_linkages();
	/* Relocation table offset setup */
	rhead.rel_table_off = code_size;
	code_size += lnkrel_cnt * SIZEOF(struct relocation_info);
	/* Symbol text list offset setup */
	rhead.sym_table_off = code_size;
	assert(OUTPUT_SYMBOL_SIZE == output_symbol_size());
	code_size += OUTPUT_SYMBOL_SIZE;
	/* Pad to OBJECT_SIZE_ALIGNMENT byte boundary.(Perhaps need for building into shared library -MR) */
	object_pad_size = PADLEN(code_size, OBJECT_SIZE_ALIGNMENT);
	code_size += object_pad_size;
	gtm_object_size = code_size;
	rhead.object_len = gtm_object_size;
	set_rtnhdr_checksum(&rhead, (gtm_rtn_src_chksum_ctx *)checksum_ctx);
	rhead.objlabel = MAGIC_COOKIE;
	rhead.temp_mvals = sa_temps[TVAL_REF];
	rhead.temp_size = sa_temps_offset[TCAD_REF];
	rhead.compiler_qlf = cmd_qlf.qlf;
	if (cmd_qlf.qlf & CQ_EMBED_SOURCE)
		rhead.routine_source_offset = TREF(routine_source_offset);
	/* Start the creation of the output object. On Linux, Solaris, and HPUX, we use ELF routines to push out native object
	 * code wrapper so the wrapper is not part of the object code hash. However, on AIX, the native XCOFF wrapper is pushed
	 * out by our own routines so is part of the hash. Note for AIX, the create_object_file() routine will duplicate the
	 * hash initialization macro below after the native header is written out but before the GT.M object header is written.
	 */
	HASH128_STATE_INIT(TREF(objhash_state), 0);
	DEFER_INTERRUPTS(INTRPT_IN_OBJECT_FILE_COMPILE, prev_intrpt_state);
	create_object_file(&rhead);
	ENABLE_INTERRUPTS(INTRPT_IN_OBJECT_FILE_COMPILE, prev_intrpt_state);
	cg_phase = CGP_MACHINE;
	code_gen();
	/* External entry definitions (line number table */
	offset = (int4)(mline_root.externalentry->rtaddr - PTEXT_OFFSET);
	emit_immed((char *)&offset, SIZEOF(offset));	/* line 0 */
	for (mlx = mline_root.child; mlx; mlx = mly)
	{
		if (mlx->table)
		{
			offset = (int4)(mlx->externalentry->rtaddr - PTEXT_OFFSET);
			emit_immed((char *)&offset, SIZEOF(offset));
		}
		if (0 == (mly = mlx->child))			/* note the assignment */
		{
			if (0 == (mly = mlx->sibling))		/* note the assignment */
			{
				for (mly = mlx; ; )
				{
					if (0 == (mly = mly->parent))	/* note the assignment */
						break;
					if (mly->sibling)
					{
						mly = mly->sibling;
						break;
					}
				}
			}
		}
	}
	if (lnr_pad_size) /* emit padding so literal text pool starts on proper boundary */
		emit_immed(PADCHARS, lnr_pad_size);
	/* Both literal text pool and literal mval table */
	emit_literals();
	/* Emit padding so variable table starts on proper boundary */
	assert(STR_LIT_LEN(PADCHARS) >= lits_pad_size);
	if (lits_pad_size)
		emit_immed(PADCHARS, lits_pad_size);
	/* Variable table: */
	vptr = (var_tabent *)mcalloc(mvmax * SIZEOF(var_tabent));
	if (mvartab)
		walktree(mvartab, cg_var, (char *)&vptr);
	emit_immed((char *)vptr, mvmax * SIZEOF(var_tabent));
	/* The label table */
	if (mlabtab)
		walktree((mvar *)mlabtab, cg_lab, (char *)TRUE);
	resolve_sym();		/* Associate relocation entries for the same symbol/linkage-table slot together */
	output_relocation();	/* Output relocation entries for the linkage section */
	output_symbol();	/* Output the symbol table text pool */
	/* If there is padding we need to do to fill out the object to the required boundary do it.. */
	if (object_pad_size)
	{
		assert(STR_LIT_LEN(PADCHARS) >= object_pad_size);
		emit_immed(PADCHARS, object_pad_size);
	}
	DEFER_INTERRUPTS(INTRPT_IN_OBJECT_FILE_COMPILE, prev_intrpt_state);
	finish_object_file();	/* Flushes object file buffers and writes remaining native object structures */
	ENABLE_INTERRUPTS(INTRPT_IN_OBJECT_FILE_COMPILE, prev_intrpt_state);
	/* Get our 128 bit hash though only the first 8 bytes of it get saved in the routine header */
	gtmmrhash_128_result(TADR(objhash_state), gtm_object_size, &objhash);
	DBGARLNK((stderr, "obj_code: Computed hash value of 0x"lvaddr" for file %.*s\n", objhash.one, object_name_len,
		  object_file_name));
	if ((off_t)-1 == lseek(object_file_des, (off_t)(NATIVE_HDR_LEN + OFFSETOF(rhdtyp, objhash)), SEEK_SET))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5) ERR_OBJFILERR, 2, object_name_len, object_file_name, errno);
	emit_immed((char *)&objhash.one, SIZEOF(gtm_uint64_t));	/* Update 8 bytes of objhash in the file header */
	buff_flush();						/* Push it out */
	CLOSE_OBJECT_FILE(object_file_des, status);
	if (-1 == status)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_SYSCALL, 5, RTS_ERROR_LITERAL("close()"), CALLFROM, errno);
	/* Ready to make object visible. Rename from tmp name to real routine name */
	RENAME_TMP_OBJECT_FILE(object_file_name);
}

/* Routine called to process a given label. Cheezy 2nd parm is due to general purpose
 * mechanism of the walktree routine that calls us.
 */
STATICFNDEF void cg_lab(mlabel *mlbl, char *do_emit)
{
	lab_tabent	lent;
	mstr		glob_name;

	if (mlbl->ml && mlbl->gbl)
	{
		if (do_emit)
		{	/* Output (2nd) pass, emit the interesting information */
			lent.lab_name.len = mlbl->mvname.len;
			lent.lab_name.addr = (0 < lent.lab_name.len)			/* Offset into literal text pool */
				? (char *)(mlbl->mvname.addr - (char *)stringpool.base) : NULL;
			lent.LABENT_LNR_OFFSET = (lnr_tabent *)(SIZEOF(lnr_tabent) * mlbl->ml->line_number);
											/* Offset into lnr table */
			lent.has_parms = (NO_FORMALLIST != mlbl->formalcnt);		/* Flag to indicate any formallist */
			GTM64_ONLY(lent.filler = 0);					/* Remove garbage due so hashes well */
			UNICODE_ONLY(lent.lab_name.char_len = 0);			/* .. ditto .. */
			emit_immed((char *)&lent, SIZEOF(lent));
		} else
		{	/* 1st pass, do the definition but no emissions */
			mlabel2xtern(&glob_name, &int_module_name, &mlbl->mvname);
			define_symbol(GTM_CODE, &glob_name);
		}
	}
	return;
}
