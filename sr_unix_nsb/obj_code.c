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

GBLREF bool		run_time;
GBLREF command_qualifier cmd_qlf;
GBLREF int4		mvmax, mlmax, mlitmax, psect_use_tab[], sa_temps[], sa_temps_offset[];
GBLREF mlabel 		*mlabtab;
GBLREF mline 		mline_root;
GBLREF mvar 		*mvartab;
GBLREF char		module_name[];

GBLDEF int4		curr_addr, code_size;
GBLREF char		cg_phase;	/* code generation phase */
GBLREF char		cg_phase_last;	/* previous code generation phase */

void	cg_lab (mlabel *l, int4 base);

void	obj_code (uint4 src_lines, uint4 checksum)
{
	rhdtyp		rhead;
	mline		*mlx, *mly;
	vent		*vptr;
	mstr		rname_mstr;
	error_def(ERR_TEXT);
	assert(!run_time);
	obj_init();


	/* Define the routine name global symbol. */
	rname_mstr.addr = module_name;
	rname_mstr.len = mid_len((mident *)module_name);
	define_symbol(GTM_MODULE_DEF_PSECT, rname_mstr, 0);

	memset(&rhead, 0, sizeof(rhead));
	alloc_reg();
	jmp_opto();
	curr_addr = sizeof(rhdtyp);
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

	rhead.ptext_ptr = sizeof(rhead);
	rhead.checksum = checksum;
	rhead.vartab_ptr = code_size;
	rhead.vartab_len = mvmax;
	code_size += mvmax*sizeof(vent);
	rhead.labtab_ptr = code_size;
	rhead.labtab_len = mlmax;
	code_size += mlmax * (sizeof(mident) + sizeof(int4));
	rhead.lnrtab_ptr = code_size;
	rhead.lnrtab_len = src_lines;
	rhead.label_only = !(cmd_qlf.qlf & CQ_LINE_ENTRY);
	rhead.temp_mvals = sa_temps[TVAL_REF];
	rhead.temp_size = sa_temps_offset[TCAD_REF];
	code_size += src_lines*sizeof(int4);

	create_object_file(&rhead);
	cg_phase = CGP_MACHINE;
	code_gen();

	/* Variable table: */
	vptr = (vent *)mcalloc(mvmax*sizeof(vent));
	if (mvartab)
	{
		walktree(mvartab, cg_var, (char *)&vptr);
	}
	emit_immed((char *)vptr, mvmax*sizeof(vent));

	/* Label table: */
	if (mlabtab)
	{
		walktree((mvar *)mlabtab, cg_lab, (char *)rhead.lnrtab_ptr);
	}

	/* External entry definitions: */
	emit_immed((char *)&(mline_root.externalentry->rtaddr), sizeof(mline_root.externalentry->rtaddr));	/* line 0 */
	for (mlx = mline_root.child ; mlx ; mlx = mly)
	{
		if (mlx->table)
			emit_immed((char *)&(mlx->externalentry->rtaddr), sizeof(mlx->externalentry->rtaddr));
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
#if !defined(__MVS__) && !defined(__s390__)	/* assert not valid for instructions on OS390 */
	assert (code_size == psect_use_tab[GTM_CODE]);
#endif
	emit_literals();
	close_object_file();
}


GBLREF char		module_name[];

void	cg_lab (mlabel *l, int4 base)
{
	mstr	glob_name;
	int4	value;

	if (l->ml  &&  l->gbl)
	{
		emit_immed(l->mvname.c, sizeof(mident));
		value = sizeof(int4)*l->ml->line_number + base;
		emit_immed((char *)&value, sizeof(value));
		mlabel2xtern(&glob_name, (mident *)module_name, &l->mvname);
		define_symbol(GTM_CODE, glob_name, value);
	}
	return;
}
