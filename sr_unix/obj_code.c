/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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

GBLDEF uint4 			lits_text_size, lits_mval_size;

IA64_DEBUG_ONLY(GBLREF int4 	generated_count;)
IA64_DEBUG_ONLY(GBLREF int4	calculated_count;)
IA64_DEBUG_ONLY(GBLREF struct inst_count 	generated_details[MAX_CODE_COUNT];)
IA64_DEBUG_ONLY(GBLREF struct inst_count	calculated_details[MAX_CODE_COUNT];)
IA64_ONLY(GBLREF int4      	generated_code_size;)
IA64_ONLY(GBLREF int4		calculated_code_size;)
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

#define PTEXT_OFFSET SIZEOF(rhdtyp)

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
 *	+ - - - - - - - +  /                               |- R/O releasable
 *	| lit text pool	| /                                |
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
 * also releasable.
 *
 * If GTM_DYNAMIC_LITERALS is enabled, the literal mval table becomes part of the R/O-release section.
 * In the case of shared libraries, this spares each process from having to take a malloced copy the lit mval table
 * at link time (sr_unix/incr_link.c). This memory saving optimization is only available on USHBIN-supported platforms
 */

error_def(ERR_TEXT);

void cg_lab (mlabel *mlbl, char *do_emit);

void obj_code (uint4 src_lines, uint4 checksum)
{
	int		i;
	uint4		lits_pad_size, object_pad_size, lnr_pad_size;
	int4		offset;
	int4		old_code_size;
	rhdtyp		rhead;
	mline		*mlx, *mly;
	var_tabent	*vptr;

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
	IA64_ONLY(calculated_code_size = 0;)
	IA64_DEBUG_ONLY(calculated_count = 0;)
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	comp_lits(&rhead);
	old_code_size = code_size;
	shrink_trips();
	IA64_ONLY
	(
		if (old_code_size != code_size)
			calculated_code_size -= ((old_code_size - code_size)/16);
	)
	if ((cmd_qlf.qlf & CQ_MACHINE_CODE))
	{
		cg_phase = CGP_ASSEMBLY;
		code_gen();
	}
	if (!(cmd_qlf.qlf & CQ_OBJECT))
		return;
	/* Executable code offset setup */
	IA64_ONLY(assert(!(PTEXT_OFFSET % SECTION_ALIGN_BOUNDARY));)
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
	rhead.checksum = checksum;
	rhead.objlabel = MAGIC_COOKIE;
	rhead.temp_mvals = sa_temps[TVAL_REF];
	rhead.temp_size = sa_temps_offset[TCAD_REF];
	rhead.compiler_qlf = cmd_qlf.qlf;
	/* Start the creation of the output object */
	create_object_file(&rhead);
	cg_phase = CGP_MACHINE;
	IA64_ONLY(generated_code_size = 0;)
	IA64_DEBUG_ONLY(generated_count = 0;)
	code_gen();
	IA64_DEBUG_ONLY(
	if (calculated_code_size != generated_code_size)
	{
		if (getenv("DUMP_CODE_SIZE_DIFFERENCE"))
		{
			PRINTF("Here is the instruction count for each triple type :\n");
			for (i = 0; i < MAX(calculated_count, generated_count); i++)
			{
				if (calculated_details[i].size != generated_details[i].size ||
				    calculated_details[i].sav_in != generated_details[i].sav_in)
					PRINTF("[X]");
				else
					PRINTF("[ ]");
			}
			PRINTF("calculated (size = %d save_in = 0x%x)  gen (size = %d save_in = 0x%x)\n",
			       calculated_details[i].size, calculated_details[i].sav_in, generated_details[i].size,
			       generated_details[i].sav_in);
		} else
			PRINTF("Set the env variable - DUMP_CODE_SIZE_DIFFERENCE - to see more details about the difference\n");
	}
	)
	IA64_ONLY(assert(calculated_code_size == generated_code_size));
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
	/* Resolve locally defined symbols */
	resolve_sym();
	/* Output relocation entries for the linkage section */
	output_relocation();
	/* Output the symbol table text pool */
	output_symbol();
	/* If there is padding we need to do to fill out the object to the required boundary do it.. */
	if (object_pad_size)
	{
		assert(STR_LIT_LEN(PADCHARS) >= object_pad_size);
		emit_immed(PADCHARS, object_pad_size);
	}
	close_object_file();
}

/* Routine called to process a given label. Cheezy 2nd parm is due to general purpose
 * mechanism of the walktree routine that calls us.
 */
void	cg_lab(mlabel *mlbl, char *do_emit)
{
	lab_tabent	lent;
	mstr		glob_name;

	if (mlbl->ml && mlbl->gbl)
	{
		if (do_emit)
		{	/* Output (2nd) pass, emit the interesting information */
			lent.lab_name.len = mlbl->mvname.len;
			lent.lab_name.addr = (char *)(mlbl->mvname.addr - (char *)stringpool.base);
											/* Offset into literal text pool */
			lent.LABENT_LNR_OFFSET = (lnr_tabent *)(SIZEOF(lnr_tabent) * mlbl->ml->line_number);
											/* Offset into lnr table */
			lent.has_parms = (NO_FORMALLIST != mlbl->formalcnt);		/* Flag to indicate any formallist */
			emit_immed((char *)&lent, SIZEOF(lent));
		} else
		{	/* 1st pass, do the definition but no emissions */
			mlabel2xtern(&glob_name, &int_module_name, &mlbl->mvname);
			define_symbol(GTM_CODE, &glob_name);
		}
	}
	return;
}
