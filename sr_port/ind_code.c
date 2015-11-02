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
#include "compiler.h"
#include "objlabel.h"
#include <rtnhdr.h>
#include "cache.h"
#include "cgp.h"
#include "stringpool.h"
#include "copy.h"
#include "mmemory.h"
#include "obj_file.h"
#include "cg_var.h"

GBLREF boolean_t	run_time;
GBLREF int4		mvmax, mlitmax;
GBLREF mvar		*mvartab;
GBLREF int4		curr_addr, code_size;
GBLREF int4		sa_temps[];
GBLREF int4		sa_temps_offset[];
GBLREF char		cg_phase;	/* code generation phase */
GBLREF char		cg_phase_last; 	/* previous code generation phase */
GBLREF spdesc		stringpool, indr_stringpool;
#ifdef __ia64
GBLREF int4		generated_code_size, calculated_code_size;
#endif

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
 *	| hdr_offset (4-byte)     | (8-byte on GTM64)
 *	+--------------------------
 *	| validation (4-byte)     | (8-byte on GTM64)
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
	uint4		indir_code_size;
	IA64_ONLY(int	old_code_size;)
	INTPTR_T	validation, hdr_offset, long_temp;
	unsigned char	*indr_base_addr;

	assert(run_time);
	curr_addr = SIZEOF(ihdtyp);
	cg_phase = CGP_APPROX_ADDR;
	cg_phase_last = CGP_NOSTATE;
	IA64_ONLY(calculated_code_size = 0);
	code_gen();
	code_size = curr_addr;
	cg_phase = CGP_ADDR_OPT;
#	if (!defined(USHBIN_SUPPORTED) && !defined(VMS))  /* non-shared binary UNIX platforms */
	shrink_jmps();
#	endif
	/* GTM64: assuming indir_code_size wont exceed MAX_INT */
	indir_code_size =
		(uint4)((SECTION_ALIGN_BOUNDARY - 1)	/* extra padding to align the beginning of the entire indirect object */
		+ SIZEOF(ihdtyp) + PADLEN(SIZEOF(ihdtyp), NATIVE_WSIZE) /* extra padding to align the beginning of lit text pool */
		+ ROUND_UP2(indr_stringpool.free - indr_stringpool.base, NATIVE_WSIZE)	/*  base literal strings */
		+ mlitmax * SIZEOF(mval)		/* literal mval table aligned at NATIVE_WSIZE boundary */
		+ (SIZEOF(INTPTR_T) * 2)		/* validation word and (neg) offset to ihdtyp */
							/* SIZEOF(INTPTR_T) is used for alignment reasons */
		+ GTM64_ONLY((SECTION_ALIGN_BOUNDARY - 1))	/* extra padding to align the beginning of the code address */
		+ code_size				/* code already aligned at SECTION_ALIGN_BOUNDARY boundary */
		+ mvmax * SIZEOF(var_tabent));		/* variable table ents */
	ENSURE_STP_FREE_SPACE(indir_code_size);
	/* Align the beginning of the indirect object so that ihdtyp fields can be accessed normally */
	stringpool.free = (unsigned char *)ROUND_UP2((UINTPTR_T)stringpool.free, SECTION_ALIGN_BOUNDARY);
	itext = (ihdtyp *)stringpool.free;
	indr_base_addr = stringpool.free;
	stringpool.free += SIZEOF(ihdtyp);
	indir_lits(itext);
	/* Runtime base (fp->ctxt) needs to be set to the beginning of the Executable code so that
	 * the literal references are generated with appropriate (-ve) offsets from the base
	 * register (fp->ctxt). On USHBIN_SUPPORTED platforms, runtime_base should be computed
	 * before shrink_trips since it could be used in codegen of literals
	 */
	runtime_base = stringpool.free + SIZEOF(hdr_offset) + SIZEOF(validation);
	/* Align the begining of the code so that it can be access properly. */
	GTM64_ONLY(runtime_base = (unsigned char *)ROUND_UP2((UINTPTR_T)runtime_base, SECTION_ALIGN_BOUNDARY);)
	IA64_ONLY(old_code_size = code_size;)
#	if defined(USHBIN_SUPPORTED) || defined(VMS)
	shrink_trips();
#	endif
	IA64_ONLY(
		if (old_code_size != code_size)
		  calculated_code_size -= ((old_code_size - code_size)/16);
	)
	/* Alignment for the starting of code address before code_gen.*/
	GTM64_ONLY(stringpool.free = (unsigned char *)ROUND_UP2((UINTPTR_T)stringpool.free, SECTION_ALIGN_BOUNDARY);)
	assert(0 == GTM64_ONLY(PADLEN((UINTPTR_T)stringpool.free, SECTION_ALIGN_BOUNDARY))
		NON_GTM64_ONLY(PADLEN((UINTPTR_T)stringpool.free, SIZEOF(int4))));
	/* Since we know stringpool is aligned atleast at 4-byte boundary, copy both offset and validation
	 * words with integer assignments instead of copying them by emit_immed(). */
	hdr_offset = indr_base_addr - stringpool.free;		/* -ve offset to ihdtyp */
	*(INTPTR_T *)stringpool.free = hdr_offset;
	stringpool.free += SIZEOF(hdr_offset);
	validation = MAGIC_COOKIE;			/* Word to validate we are in right place */
	*(UINTPTR_T *)stringpool.free = validation;
	stringpool.free += SIZEOF(validation);
	cg_phase = CGP_MACHINE;
	IA64_ONLY(generated_code_size = 0);
	code_gen();
	IA64_ONLY(assert(calculated_code_size == generated_code_size));
	long_temp = stringpool.free - indr_base_addr;
	assert(0 == PADLEN(long_temp, SIZEOF(INTPTR_T))); /* Just to make sure things are aligned for the vartab that follows */
	/* variable table */
	itext->vartab_off = (int4)long_temp;
	itext->vartab_len = mvmax;
	vptr = (var_tabent*)mcalloc(mvmax * SIZEOF(var_tabent));
	if (mvartab)
		walktree(mvartab, ind_cg_var, (char *)&vptr);
	else
		assert(0 == mvmax);
	emit_immed((char *) vptr, mvmax * SIZEOF(var_tabent));
	itext->temp_mvals = sa_temps[TVAL_REF];
	itext->temp_size = sa_temps_offset[TCAD_REF];
	/* indir_code_size may be greater than the actual resultant code size because expression coersion may cause some literals
	 * to be optimized away, leaving mlitmax greater than actual.
	 */
	assert(indir_code_size >= stringpool.free - indr_base_addr);
	/* Return object code pointers on stack. A later cache_put will move
	 * the code to its new home and do the necessary cleanup on it.
	 */
	obj->addr = (char *)indr_base_addr;
	obj->len = INTCAST(stringpool.free - indr_base_addr);
}
