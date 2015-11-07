/****************************************************************
 *								*
 *	Copyright 2001, 2012 Fidelity Information Services, Inc	*
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
#include <sys/types.h>
#include "gtm_stat.h"
#include "gtm_unistd.h"

#include "compiler.h"
#include "obj_gen.h"
#include <rtnhdr.h>
#include "cmd_qlf.h"
#include "cgp.h"
#include "mmemory.h"
#include "obj_file.h"
#include "alloc_reg.h"
#include "jmp_opto.h"
#include "mlabel2xtern.h"
#include "cg_var.h"
#include "gtm_string.h"
#include "stringpool.h"

GBLREF boolean_t		run_time;
GBLREF command_qualifier	cmd_qlf;
GBLREF int4			mvmax, mlmax, mlitmax, psect_use_tab[], sa_temps[], sa_temps_offset[];
GBLREF mlabel 			*mlabtab;
GBLREF mline 			mline_root;
GBLREF mvar 			*mvartab;
GBLREF mident			module_name;
GBLREF spdesc			stringpool;
GBLREF int4			curr_addr, code_size;
GBLREF char			cg_phase;	/* code generation phase */
GBLREF char			cg_phase_last;	/* previous code generation phase */

error_def(ERR_TEXT);

void cg_lab (mlabel *l, int4 base);

/* The sections of the internal GT.M object are grouped according to native sections.
 * Note: Once an object is linked, no section will be released from memory. All sections
 * will be retained.
 *
 * The GT.M object layout on the disk is as follows:
 *
 *	+---------------+
 *	|     rhead	| \
 *	+---------------+  \
 *	|   generated	|   |
 *	|     code	|   |
 *	+ - - - - - - - +   |
 *	| variable tbl	|   | - R/O (GTM$CODE psect)
 *	+ - - - - - - - +   |
 *	|   label tbl	|   |
 *	+---------------+  /
 *	| line num tbl	| /
 *	+ - - - - - - - +
 *	| lit text pool	| \
 *	+---------------+  | - R/W (GTM$LITERALS)
 *	| lit mval tbl 	| /
 *	+---------------+
 *	| GTM$Rxxx      | > - R/W (GTM$Rxx psect)
 *	+---------------+
 *	|  symbol tbl 	| > - External symbol table
 *	+---------------+
 *
 */

void	obj_code (uint4 src_lines, uint4 checksum)
{
	rhdtyp		rhead;
	mline		*mlx, *mly;
	var_tabent	*vptr;
	mstr		rname_mstr;
	int4		mv, lnr_pad_len;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	assert(!run_time);
	obj_init();

	/* Define the routine name global symbol. */
	rname_mstr.addr = module_name.addr;
	rname_mstr.len = module_name.len;
	define_symbol(GTM_MODULE_DEF_PSECT, &rname_mstr, 0);
	memset(&rhead, 0, SIZEOF(rhead));
	alloc_reg();
	jmp_opto();
	curr_addr = SIZEOF(rhdtyp);
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	comp_lits(&rhead);
	shrink_trips();
	if ((cmd_qlf.qlf & CQ_MACHINE_CODE))
	{
		cg_phase = CGP_ASSEMBLY;
		code_gen();
	}
	if (!(cmd_qlf.qlf & CQ_OBJECT))
		return;
	rhead.ptext_ptr = SIZEOF(rhead);
	rhead.checksum = checksum;
	rhead.vartab_ptr = code_size;
	rhead.vartab_len = mvmax;
	code_size += mvmax * SIZEOF(var_tabent);
	rhead.labtab_ptr = code_size;
	rhead.labtab_len = mlmax;
	code_size += mlmax * SIZEOF(lab_tabent);
	rhead.lnrtab_ptr = code_size;
	rhead.lnrtab_len = src_lines;
	rhead.compiler_qlf = cmd_qlf.qlf;
	rhead.temp_mvals = sa_temps[TVAL_REF];
	rhead.temp_size = sa_temps_offset[TCAD_REF];
	code_size += src_lines * SIZEOF(int4);
	lnr_pad_len = PADLEN(code_size, SECTION_ALIGN_BOUNDARY);
	code_size += lnr_pad_len;
	create_object_file(&rhead);
	cg_phase = CGP_MACHINE;
	code_gen();
	/* Variable table: */
	vptr = (var_tabent *)mcalloc(mvmax * SIZEOF(var_tabent));
	if (mvartab)
		walktree(mvartab, cg_var, (char *)&vptr);
	else
		assert(0 == mvmax);
	/* Although entire vartab is available, we cannot emit the table in one chunk using emit_immed()
	 * since var_name.addr of each entry needs relocation and should be emitted using emit_pidr()
	 * strictly in that order to maintain proper relocation base for fixup generation.
	 */
	for (mv = 0; mv < mvmax; mv++)
	{
		emit_immed((char *)&vptr[mv], ((char *)&vptr[mv].var_name.addr - (char *)&vptr[mv]));
		emit_pidr((int4)vptr[mv].var_name.addr, GTM_LITERALS);
		emit_immed(&vptr[mv].hash_code, SIZEOF(vptr[mv].hash_code));
		emit_immed(&vptr[mv].marked, SIZEOF(vptr[mv].marked));
	}
	/* Label table: */
	if (mlabtab)
	{
		TREF(lbl_tbl_entry_index) = -1;	/* this is incremented by 1 each time a label is emitted */
		walktree((mvar *)mlabtab, cg_lab, (char *)&rhead);
	} else
		assert(0 == mlmax);
	/* External entry definitions: */
	emit_immed((char *)&(mline_root.externalentry->rtaddr), SIZEOF(mline_root.externalentry->rtaddr));	/* line 0 */
	for (mlx = mline_root.child ; mlx ; mlx = mly)
	{
		if (mlx->table)
			emit_immed((char *)&(mlx->externalentry->rtaddr), SIZEOF(mlx->externalentry->rtaddr));
		if (0 == (mly = mlx->child))				/* note the assignment */
			if (0 == (mly = mlx->sibling))			/* note the assignment */
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
	if (0 != lnr_pad_len) /* emit padding so literal text pool starts on proper boundary */
		emit_immed(PADCHARS, lnr_pad_len);
	emit_literals();
	close_object_file(&rhead);
}

void cg_lab (mlabel *l, int4 base)
{
	mstr		glob_name;
	int4		value;
	boolean_t	has_parms;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	if (l->ml && l->gbl)
	{
		if (l->mvname.len)
		{	/* Non-null label: emit relocation */
			emit_immed((char *)&l->mvname.len, SIZEOF(l->mvname.len));
			emit_pidr((l->mvname.addr - (char *)stringpool.base), GTM_LITERALS); /* Offset into literal text pool */
		} else
			/* Null label: no relocation needed, emit mident as it is */
			emit_immed((char *)&l->mvname, SIZEOF(l->mvname));
		value = (SIZEOF(int4) * l->ml->line_number) + ((rhdtyp *)base)->lnrtab_ptr;
		emit_immed((char *)&value, SIZEOF(value));
		has_parms = (NO_FORMALLIST != l->formalcnt);	/* Flag to indicate a formallist */
		emit_immed((char *)&has_parms, SIZEOF(has_parms));
		mlabel2xtern(&glob_name, &module_name, &l->mvname);
		(TREF(lbl_tbl_entry_index))++;		/* Find out the index of this label in the label table */
		/* Define this symbol by calculating the offset of lab_ln_ptr field of the current label relatively to
		 * the routine header.
		 */
		define_symbol(GTM_CODE, &glob_name, ((rhdtyp *)base)->labtab_ptr
			+ (SIZEOF(lab_tabent) * TREF(lbl_tbl_entry_index)) + OFFSETOF(lab_tabent, lab_ln_ptr));
	}
	return;
}
