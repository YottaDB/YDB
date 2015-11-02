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
#include "cache.h"
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
GBLREF spdesc	stringpool, indr_stringpool;

GBLDEF unsigned char	*runtime_base;

/**************  Indirect Object Code Format *************
 *
 *	+-------------------------+> aligned section boundary
 *	| ihdtyp                  |
 *	+-------------------------> aligned section boundary
 *	| lit text pool           |
 *	+--------------------------
 *	| lit mval table          |
 *	+--------------------------
 *	| hdr_offset (4-byte)     |
 *	+--------------------------
 *	| validation (4-byte)     |
 *	+--------------------------
 *	| Executable Code         |
 *	+--------------------------
 *	| variable Table          |
 *	+-------------------------+
 *
 ***************************************************/

void	ind_code(mstr *obj)
{
	var_tabent	*vptr;
	ihdtyp		*itext;
	int		indir_code_size;
	int4		validation, hdr_offset, long_temp;
	unsigned char	*indr_base_addr;

	assert(run_time);
	curr_addr = sizeof(ihdtyp);
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
#if (!defined(USHBIN_SUPPORTED) && !defined(VMS))  /* non-shared binary UNIX platforms */
	shrink_jmps();
#endif
	indir_code_size =
		(SECTION_ALIGN_BOUNDARY - 1) +	/* extra padding to align the beginning of the entire indirect object */
		PADLEN(sizeof(ihdtyp), NATIVE_WSIZE) + /* extra padding to align the beginning of lit text pool */
		ROUND_UP2(indr_stringpool.free - indr_stringpool.base, NATIVE_WSIZE) +	/* literal strings */
		mlitmax * sizeof(mval) +	/* literal mval table aligned at NATIVE_WSIZE boundary */
		(sizeof(int4) * 2) +		/* validation word and (neg) offset to ihdtyp */
		code_size +			/* code already aligned at SECTION_ALIGN_BOUNDARY boundary */
		mvmax * sizeof(var_tabent);	/* variable table ents */
	if (stringpool.top - stringpool.free < indir_code_size)
		stp_gcol(indir_code_size);
	/* Align the beginning of the indirect object so that ihdtyp fields can be accessed normally */
	stringpool.free = (unsigned char *)ROUND_UP2((uint4)stringpool.free, SECTION_ALIGN_BOUNDARY);
	itext = (ihdtyp *)stringpool.free;
	indr_base_addr = stringpool.free;
	stringpool.free += sizeof(ihdtyp);
	indir_lits(itext);
	/* Runtime base (fp->ctxt) needs to be set to the beginning of the Executable code so that
	 * the literal references are generated with appropriate (-ve) offsets from the base
	 * register (fp->ctxt). On USHBIN_SUPPORTED platforms, runtime_base should be computed
	 * before shrink_trips since it could be used in codegen of literals */
	runtime_base = stringpool.free + sizeof(hdr_offset) + sizeof(validation);
#if (defined(USHBIN_SUPPORTED) || defined(VMS))
	shrink_trips();
#endif
	assert(0 == PADLEN((uint4)stringpool.free, sizeof(int4)));
	/* Since we know stringpool is aligned atleast at 4-byte boundary, copy both offset and validation
	 * words with integer assignments instead of copying them by emit_immed(). */
	hdr_offset = indr_base_addr - stringpool.free;		/* -ve offset to ihdtyp */
	*(int4 *)stringpool.free = hdr_offset;
	stringpool.free += sizeof(hdr_offset);
	validation = MAGIC_COOKIE;			/* Word to validate we are in right place */
	*(int4 *)stringpool.free = validation;
	stringpool.free += sizeof(validation);

	cg_phase = CGP_MACHINE;
	code_gen();
	long_temp = stringpool.free - indr_base_addr;
	assert(0 == PADLEN(long_temp, sizeof(int4))); /* Just to make sure things are aligned for the vartab that follows */
	/* variable table */
	itext->vartab_off = long_temp;
	itext->vartab_len = mvmax;
	vptr = (var_tabent*)mcalloc(mvmax * sizeof(var_tabent));
	if (mvartab)
		walktree(mvartab, ind_cg_var, (char *)&vptr);
	else
		assert(0 == mvmax);
	emit_immed((char *) vptr, mvmax * sizeof(var_tabent));

	itext->temp_mvals = sa_temps[TVAL_REF];
	itext->temp_size = sa_temps_offset[TCAD_REF];
	/* indir_code_size may be greater than the actual resultant code size
	   because expression coersion may cause some literals to be optimized
	   away, leaving mlitmax greater than actual.
	*/
	assert(indir_code_size >= stringpool.free - indr_base_addr);
	/* Return object code pointers on stack. A later cache_put will move
	   the code to its new home and do the necessary cleanup on it.
	*/
	obj->addr = (char *)indr_base_addr;
	obj->len = stringpool.free - indr_base_addr;
}
