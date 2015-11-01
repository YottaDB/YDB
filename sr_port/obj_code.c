/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
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

GBLREF bool		run_time;
GBLREF command_qualifier cmd_qlf;
GBLREF int4		mvmax, mlmax, mlitmax, psect_use_tab[], sa_temps[], sa_temps_offset[];
GBLREF mlabel 		*mlabtab;
GBLREF mline 		mline_root;
GBLREF mvar 		*mvartab;
GBLREF char		module_name[];

GBLDEF int4		curr_addr, code_size;
GBLDEF char		cg_phase;	/* code generation phase */
GBLDEF int		stdin_fd;
GBLDEF int		stderr_fd;
GBLDEF int		stdout_fd;
#if defined(__osf__) && defined(DEBUG)
char	tmpfile_name[1024];
#endif

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
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	shrink_jmps();
	comp_lits(&rhead);
	if ((cmd_qlf.qlf & CQ_MACHINE_CODE))
	{
#ifdef UNIX
		int dup2_res;
#endif
#if defined(__osf__) && defined(DEBUG)
		memcpy(tmpfile_name, "/tmp/output.lis",15);
		*(i2asc((uchar_ptr_t)&tmpfile_name[15], (unsigned int)getpid())) = 0;
#ifdef UNIX
		OPENFILE3(tmpfile_name, O_CREAT | O_RDWR, 0666, stderr_fd);
		if (-1 == stderr_fd)
#else
		if ((stderr_fd = OPEN3(tmpfile_name, O_CREAT | O_RDWR, 0666)) == -1)
#endif
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Error in open in obj_code"), errno);

		stdin_fd = stdout_fd = stderr_fd;

#ifdef UNIX
		DUP2(stderr_fd, 0, dup2_res);
		if (-1 == dup2_res)
#else
		if (dup2(stderr_fd, 0) == -1)
#endif
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Error in dup2(stdin) in obj_code"), errno);

#ifdef UNIX
		DUP2(stderr_fd, 2, dup2_res);
		if (-1 == dup2_res)
#else
		if (dup2(stderr_fd, 2) == -1)
#endif
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Error in dup2(stderr) in obj_code"), errno);

#ifdef UNIX
		DUP2(stdout_fd, 1, dup2_res);
		if (-1 == dup2_res)
#else
		if (dup2(stdout_fd, 1) == -1)
#endif
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Error in dup2(stdout) in obj_code"), errno);
#endif
		cg_phase = CGP_ASSEMBLY;
		code_gen();

#if defined(__osf__) && defined(DEBUG)
		if (UNLINK(tmpfile_name) == -1)
			rts_error(VARLSTCNT(5) ERR_TEXT, 2, RTS_ERROR_TEXT("Error in unlink in obj_code"), errno);
#endif
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
#ifndef __MVS__		/* assert not valid for instructions on OS390 */
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
