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
#include "gtm_stat.h"
#include <sys/types.h>
#include "gtm_unistd.h"

#include "compiler.h"
#include "obj_gen.h"
#include <rtnhdr.h>
#include "cmd_qlf.h"
#include "cgp.h"
#ifdef UNIX
#include "gtmio.h"
#include "eintr_wrappers.h"
#endif
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
GBLREF mident			module_name, int_module_name;
GBLREF spdesc			stringpool;
GBLREF char			cg_phase;	/* code generation phase */
GBLREF char			cg_phase_last;	/* previous code generation phase */
GBLREF int4			curr_addr, code_size;

error_def(ERR_TEXT);

void	cg_lab (mlabel *l, int4 base);

/* The sections of the internal GT.M object are grouped according to their type (R/O, R/W).
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
 *	| variable tbl	|   | - R/O
 *	+ - - - - - - - +   |
 *	|   label tbl	|   |
 *	+---------------+   |
 *	| line num tbl	|   |
 *	+ - - - - - - - +  /
 *	| lit text pool	| /
 *	+---------------+
 *	| lit mval tbl 	|-- R/W
 *	+---------------+
 *	| relocations	| > - relocations for external syms (not kept after link)
 *	+---------------+
 *	|  symbol tbl 	| > - external symbol table (not kept after link)
 *	+---------------+
 *
 */

void	obj_code (uint4 src_lines, uint4 checksum)
{
	rhdtyp		rhead;
	mline		*mlx, *mly;
	var_tabent	*vptr;
	int4		lnr_pad_len;

	assert(!run_time);
	obj_init();
	/* Define the routine name global symbol. */
	define_symbol(GTM_MODULE_DEF_PSECT, (mstr *)&int_module_name, 0);
	memset(&rhead, 0, SIZEOF(rhead));
	alloc_reg();
	jmp_opto();
	curr_addr = SIZEOF(rhdtyp);
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
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
	emit_immed((char *)vptr, mvmax * SIZEOF(var_tabent));
	/* Label table: */
	if (mlabtab)
		walktree((mvar *)mlabtab, cg_lab, (char *)rhead.lnrtab_ptr);
	else
		assert(0 == mlmax);
	/* External entry definitions: */
	emit_immed((char *)&(mline_root.externalentry->rtaddr), SIZEOF(mline_root.externalentry->rtaddr));	/* line 0 */
	for (mlx = mline_root.child; mlx; mlx = mly)
	{
		if (mlx->table)
			emit_immed((char *)&(mlx->externalentry->rtaddr), SIZEOF(mlx->externalentry->rtaddr));
		if (0 == (mly = mlx->child))				/* note assignment */
			if (0 == (mly = mlx->sibling))			/* note assignment */
				for (mly = mlx;  ;  )
				{
					if (0 == (mly = mly->parent))	/* note assignment */
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
#if !defined(__MVS__) && !defined(__s390__)	/* assert not valid for instructions on OS390 */
	assert(code_size == psect_use_tab[GTM_CODE]);
#endif
	emit_literals();
	close_object_file();
}

void	cg_lab (mlabel *l, int4 base)
{
	mstr		glob_name;
	lab_tabent	lent;

	if (l->ml && l->gbl)
	{
		lent.lab_name.len = l->mvname.len;
		lent.lab_name.addr = (char *)(l->mvname.addr - (char *)stringpool.base);
		lent.LABENT_LNR_OFFSET = (SIZEOF(lnr_tabent) * l->ml->line_number) + base;
		lent.has_parms = (NO_FORMALLIST != l->formalcnt);	/* Flag to indicate a formallist */
		emit_immed((char *)&lent, SIZEOF(lent));
		mlabel2xtern(&glob_name, &int_module_name, &l->mvname);
		define_symbol(GTM_CODE, &glob_name, lent.LABENT_LNR_OFFSET);
	}
}
