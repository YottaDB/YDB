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
#include "compiler.h"
#include "objlabel.h"
#include "rtnhdr.h"
#include "cgp.h"
#include "stringpool.h"
#include "copy.h"
#include "mmemory.h"
#include "obj_file.h"
#include "cg_var.h"

GBLREF bool	run_time;
GBLREF int4	mvmax, mlitmax;
GBLREF mvar 	*mvartab;
GBLREF int4	curr_addr, code_size;
GBLREF int4	sa_temps[];
GBLREF int4	sa_temps_offset[];
GBLREF char	cg_phase;	/* code generation phase */
GBLREF char	cg_phase_last; 	/* previous code generation phase */
GBLREF spdesc	stringpool,indr_stringpool;

GBLDEF unsigned char	*runtime_base;

void	ind_code(mstr *obj)
{
	VAR_TABENT	*vptr;
	ihdtyp		*itext;
	int		indir_code_size, validation, hdr_offset;
	int4		long_temp;
	uint4		ulong_temp;
	unsigned char	*indr_base_addr;

	assert(run_time);
	curr_addr = sizeof(ihdtyp);
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
	indir_code_size = indr_stringpool.free - indr_stringpool.base +	/* literal strings */
		mlitmax * sizeof(mval) +			/* literal mvals */
		mvmax * sizeof(VAR_TABENT) +				/* variable table ents */
		code_size +
		(sizeof(int4) * 2) +				/* validation word and (neg) offset to ihdtyp */
		(SECTION_ALIGN_BOUNDARY - 1);						/* Max amount for code alignment */
	if (stringpool.top - stringpool.free < indir_code_size)
		stp_gcol(indir_code_size);
	itext = (ihdtyp *)stringpool.free;
	indr_base_addr = stringpool.free;
	stringpool.free += sizeof(ihdtyp);
	indir_lits(itext);
	/* Runtime base needs to be set to approximately what it would have been set to below for shrink_trips to work
	   correctly so do that here */
	runtime_base = stringpool.free + sizeof(hdr_offset) + sizeof(validation);
	shrink_trips();
	hdr_offset = indr_base_addr - stringpool.free;		/* offset to ihdtyp */
	emit_immed((char *)&hdr_offset, sizeof(hdr_offset));
	validation = (GTM_OMAGIC << 16) + OBJ_LABEL;			/* Word to validate we are in right place */
	emit_immed((char *)&validation, sizeof(validation));
	/* runtime_base = stringpool.free; */
	cg_phase = CGP_MACHINE;
	code_gen();

	/* itext->vartab_off = stringpool.free - indr_base_addr; */
	long_temp = stringpool.free - indr_base_addr;
	PUT_LONG(&(itext->vartab_off), long_temp);
	/* itext->vartab_len = mvmax; */
	long_temp = mvmax;
	PUT_LONG(&(itext->vartab_len), long_temp);
	vptr = (VAR_TABENT *) mcalloc(mvmax * sizeof(VAR_TABENT));
	if (mvartab)
		walktree(mvartab, cg_var, (char *)&vptr);
	emit_immed((char *) vptr, mvmax * sizeof(VAR_TABENT));
	/* itext->temp_mvals = sa_temps[TVAL_REF]; */
	long_temp = sa_temps[TVAL_REF];
	PUT_LONG(&(itext->temp_mvals), long_temp);
	/* itext->temp_size = sa_temps_offset[TCAD_REF]; */
	ulong_temp = sa_temps_offset[TCAD_REF];
	PUT_ULONG(&(itext->temp_size), ulong_temp);
	/* indir_code_size may be greater than the actual resultant code size
	   because expression coersion may cause some literals to be optimized
	   away, leaving mlitmax greater than actual.
	*/
	assert(indir_code_size >= stringpool.free - indr_base_addr);
	/* Return object code pointers on stack. A later cache_put will move
	   the code to its new home and do the necessary cleanup on it.
	*/
	obj->addr = (char *)indr_base_addr;
	obj->len = indir_code_size;
}
