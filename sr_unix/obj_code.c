/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
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
#include <sys/stat.h>
#include <sys/types.h>
#include "gtm_unistd.h"

#include "compiler.h"
#include "obj_gen.h"
#include "rtnhdr.h"
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

GBLDEF int4		curr_addr, code_size;
GBLDEF char		cg_phase;	/* code generation phase */

GBLREF bool		run_time;
GBLREF command_qualifier cmd_qlf;
GBLREF int4		mvmax, mlmax, mlitmax, sa_temps[], sa_temps_offset[];
GBLREF mlabel 		*mlabtab;
GBLREF mline 		mline_root;
GBLREF mvar 		*mvartab;
GBLREF char		module_name[];
GBLREF uint4 		lits_text_size, lits_mval_size;
GBLREF int4		gtm_object_size;
GBLREF int4		linkage_size;
GBLREF uint4		lnkrel_cnt;	/* number of entries in linkage Psect to relocate */

#define PTEXT_OFFSET sizeof(rhdtyp)
#define PADCHARS "PADDING PADDING"

void	cg_lab (mlabel *mlbl, char *do_emit);

void	obj_code (uint4 src_lines, uint4 checksum)
{
	uint4		lits_pad_size, object_pad_size;
	int4		offset;
	rhdtyp		rhead;
	mline		*mlx, *mly;
	VAR_TABENT	*vptr;
	mstr		rname_mstr;
	error_def(ERR_TEXT);

	assert(!run_time);
	obj_init();

	/* Define the routine name global symbol. */
	rname_mstr.addr = module_name;
	rname_mstr.len = mid_len((mident *)module_name);
	define_symbol(GTM_MODULE_DEF_PSECT, &rname_mstr);

	memset(&rhead, 0, sizeof(rhead));
	alloc_reg();
	jmp_opto();
	/* Note that this initial setting of curr_addr is historical in that the routine header was
	   contiguous with the code. This is no longer true and this address would be more correct
	   if it were set to zero however if that is done and the M code contains a branch or recursive
	   DO to the top of this routine, that branch will trigger an assert failure in emit_code
	   because a branch to "location 0" has long-jump implications. Keeping the initial offset
	   non-zero avoids this problem. We had to add PTEXT_OFFSET to curr_addr to get code_size
	   after the CGP_APPROX_ADDR phase anyway. Note that in using this offset, the rtaddr field
	   of the triples has a PTEXT_OFFSET origin so this will need to be accounted for when the
	   various tables below are generated using the rtaddr field. SE 10/2002
	*/
	curr_addr = PTEXT_OFFSET;
	cg_phase = CGP_APPROX_ADDR;
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	shrink_jmps();
	comp_lits(&rhead);
	if ((cmd_qlf.qlf & CQ_MACHINE_CODE))
	{
		cg_phase = CGP_ASSEMBLY;
		code_gen();
	}
	if (!(cmd_qlf.qlf & CQ_OBJECT))
		return;

	/* Executable code offset setup */
	rhead.ptext_adr = (unsigned char *)PTEXT_OFFSET;
	rhead.ptext_end_adr = (unsigned char *)code_size;
	/* Variable table offset setup */
	rhead.vartab_adr = (var_tabent *)rhead.ptext_end_adr;
	rhead.vartab_len = mvmax;
	code_size += mvmax * sizeof(VAR_TABENT);
	/* Line number table offset setup */
	rhead.lnrtab_adr = (lnr_tabent *)code_size;
	rhead.lnrtab_len = src_lines;
	code_size += src_lines * sizeof(int4);
	/* Literal text section offset setup */
	rhead.literal_text_adr = (unsigned char *)code_size;
	rhead.literal_text_len = lits_text_size;
	code_size += lits_text_size;
	/* Literal mval section offset setup */
	rhead.literal_adr = (mval *)code_size;
	rhead.literal_len = lits_mval_size / sizeof(mval);
	code_size += lits_mval_size;
	/* Padding so label table starts on proper boundary */
	lits_pad_size = PADLEN(code_size, sizeof(char *));
	code_size += lits_pad_size;
	/* Linkage section setup. No table in object but need its length. */
	rhead.linkage_adr = NULL;
	rhead.linkage_len = linkage_size / sizeof(lnk_tabent);
	/* Label table offset setup */
	rhead.labtab_adr = (lab_tabent *)code_size;
	rhead.labtab_len = mlmax;
	code_size += mlmax * sizeof(lab_tabent);
	/* Prior to calculating reloc and symbol size, run the linkage chains to fill out
	   the reloc table and symbol table so we get the same size we will create later.
	*/
	if (mlabtab)
		walktree((mvar *)mlabtab, cg_lab, NULL);
	comp_linkages();
	/* Relocation table offset setup */
	rhead.rel_table_off = code_size;
	code_size += lnkrel_cnt * sizeof(struct relocation_info);
	/* Symbol text list offset setup */
	rhead.sym_table_off = code_size;
	code_size += output_symbol_size();
	/* Pad to 16 byte boundary */
	object_pad_size = PADLEN(code_size, 16);
	code_size += object_pad_size;

	gtm_object_size = code_size;

	rhead.checksum = checksum;
	rhead.objlabel = MAGIC_COOKIE;
	rhead.label_only = !(cmd_qlf.qlf & CQ_LINE_ENTRY);
	rhead.temp_mvals = sa_temps[TVAL_REF];
	rhead.temp_size = sa_temps_offset[TCAD_REF];

	/* Start the creation of the output object */
	create_object_file(&rhead);
	cg_phase = CGP_MACHINE;
	code_gen();
	/* Variable table: */
	vptr = (var_tabent *)mcalloc(mvmax * sizeof(var_tabent));
	if (mvartab)
		walktree(mvartab, cg_var, (char *)&vptr);
	emit_immed((char *)vptr, mvmax * sizeof(VAR_TABENT));
	/* External entry definitions (line number table */
	offset = mline_root.externalentry->rtaddr - PTEXT_OFFSET;
	emit_immed((char *)&offset, sizeof(offset));	/* line 0 */
	for (mlx = mline_root.child ; mlx ; mlx = mly)
	{
		if (mlx->table)
		{
			offset = mlx->externalentry->rtaddr - PTEXT_OFFSET;
			emit_immed((char *)&offset, sizeof(offset));
		}
		if ((mly = mlx->child) == 0)
		{
			if ((mly = mlx->sibling) == 0)
			{
				for (mly = mlx;  ;  )
				{
					if ((mly = mly->parent) == 0)
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
	/* Both literal text pool and literal mval table */
	emit_literals();
	/* Emit padding so label section starts on proper boundary */
	assert(lits_pad_size <= strlen(PADCHARS));
	if (lits_pad_size)
		emit_immed(PADCHARS, lits_pad_size);
	/* The label table */
	if (mlabtab)
		walktree((mvar *)mlabtab, cg_lab, (char *)TRUE);
	/* Resolve locally defined symbols */
	resolve_sym();
	/* Output relocation entries for the linkage section */
	output_relocation();
	/* Output the symbol table text ppol */
	output_symbol();
	/* If there is padding we need to do to fill out the object to the required
	   boundary do it..
	*/
	if (object_pad_size)
	{
		assert(sizeof(PADCHARS) >= object_pad_size);
		emit_immed(PADCHARS, object_pad_size);
	}
	close_object_file();
}


/* Routine called to process a given label. Cheezy 2nd parm is due to general purpose
   mechanism of the walktree routine that calls us.
*/
void	cg_lab(mlabel *mlbl, char *do_emit)
{
	mstr	glob_name;
	int4	value;

	/* Note that although "value" is (ultimately) a pointer, in this context it is
	   an unrelocated offset so for clarity sake we just call it an int4 instead
	   of an (lnr_tabent *).
	*/
	if (mlbl->ml  &&  mlbl->gbl)
	{
		if (do_emit)
		{	/* Output (2nd) pass, emit the interesting information */
			value = sizeof(lnr_tabent) * mlbl->ml->line_number; /* Offset into lnr table */
			emit_immed(mlbl->mvname.c, sizeof(mident));
			emit_immed((char *)&value, sizeof(value));
		} else
		{	/* 1st pass, do the definition but no emissions */
			mlabel2xtern(&glob_name, (mident *)module_name, &mlbl->mvname);
			define_symbol(GTM_CODE, &glob_name);
		}
	}
	return;
}
