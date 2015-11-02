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

#include "gtm_string.h"

#include "error.h"
#include "dm_setup.h"
#include <rtnhdr.h>
#include "op.h"
#include "compiler.h"
#include <emit_code.h>
#include "gtmci.h"
#include "inst_flush.h"
#include "obj_file.h"
#include "gtm_text_alloc.h"

#ifdef USHBIN_SUPPORTED
/* From here down is only defined in a shared binary environment */

#include "make_mode.h"

#ifdef __ia64

#if defined(__linux__)

void dmode_table_init() __attribute__((constructor));

#else /* __hpux */

#pragma INIT "dmode_table_init"

#endif /* __linux__ */

void dmode_table_init(void);

#endif /* __ia64 */

/* This routine is called to create (currently two different) dynamic routines that
 * can be executed. One is a direct mode frame, the other is a callin base frame. They
 * are basically identical except in the entry points that they call. To this end, this
 * common routine is called for both with the entry points to be put into the generated
 * code being passed in as parameters.
 */
typedef struct dyn_modes_struct
{
	char 		*rtn_name;
	int		rtn_name_len;
	void		(*func_ptr1)(void);
	void		(*func_ptr2)(void);
	int		(*func_ptr3)(void);
} dyn_modes;

static dyn_modes our_modes[2] =
{
	{
		GTM_DMOD,
		SIZEOF(GTM_DMOD) - 1,
		dm_setup,
		mum_tstart,
		opp_ret
	},
	{
		GTM_CIMOD,
		SIZEOF(GTM_CIMOD) - 1,
		ci_restart,
		ci_ret_code,
		opp_ret
	}
};

#if defined(__ia64)

/* On IA64, we want to use CODE_ADDRESS() macro, to dereference all the function pointers, before storing them in
 * global array. Now doing a dereference operation, as part of initialization, is not allowed by linux/gcc (HP'a aCC
 * was more tolerant towards this). So to make sure that the xfer_table is initialized correctly, before anyone
 * uses it, one needs to create a 'constructor/initializer' function, which is gauranted to be called as soon as
 * this module is loaded, and initialize the xfer_table correctly within that function.  gcc provides the below
 * mechanism to do this
 */
static char dyn_modes_type[2][3] = {
					{'C','A','A'},
					{'A','C','A'}
};

void dmode_table_init()
{
	/*
	our_modes[0].func_ptr1 = (void (*)())CODE_ADDRESS(our_modes[0].func_ptr1);
	our_modes[0].func_ptr2 = (void (*)())CODE_ADDRESS(our_modes[0].func_ptr2);
	our_modes[0].func_ptr3 = (int (*)())CODE_ADDRESS(our_modes[0].func_ptr3);

	our_modes[1].func_ptr1 = (void (*)())CODE_ADDRESS(our_modes[1].func_ptr1);
	our_modes[1].func_ptr2 = (void (*)())CODE_ADDRESS(our_modes[1].func_ptr2);
	our_modes[1].func_ptr3 = (int (*)())CODE_ADDRESS(our_modes[1].func_ptr3);
	*/
}

#endif /* __ia64 */

rhdtyp *make_mode (int mode_index)
{
	rhdtyp		*base_address;
	lab_tabent	*lbl;
	lnr_tabent	*lnr;
	CODEBUF_TYPE	*code;
	dyn_modes	*dmode;
	int algnd_rtnhdr_size = (int)ROUND_UP2(SIZEOF(rhdtyp), SECTION_ALIGN_BOUNDARY);
	int algnd_code_size   = (int)ROUND_UP2(CODE_SIZE, NATIVE_WSIZE);
	int algnd_lbltab_size = (int)ROUND_UP2(SIZEOF(lab_tabent), NATIVE_WSIZE);
	int algnd_lnrtab_size = (int)ROUND_UP2(CODE_LINES * SIZEOF(lnr_tabent), NATIVE_WSIZE);

	assert((DM_MODE == mode_index) || (CI_MODE == mode_index));
        base_address = (rhdtyp *)GTM_TEXT_ALLOC(algnd_rtnhdr_size + algnd_code_size + algnd_lbltab_size + algnd_lnrtab_size);
	memset(base_address, 0, algnd_rtnhdr_size + algnd_code_size + algnd_lbltab_size + algnd_lnrtab_size);
	dmode = &our_modes[mode_index];
	base_address->routine_name.len = dmode->rtn_name_len;
	base_address->routine_name.addr = dmode->rtn_name;
	base_address->ptext_adr = (unsigned char *)base_address + algnd_rtnhdr_size;
	base_address->ptext_end_adr = (unsigned char *)base_address->ptext_adr + algnd_code_size;
	base_address->lnrtab_adr = (lnr_tabent *)base_address->ptext_end_adr;
	base_address->labtab_adr = (lab_tabent *)((unsigned char *)base_address + algnd_rtnhdr_size +
						  algnd_code_size + algnd_lnrtab_size);
	base_address->lnrtab_len = CODE_LINES;
	base_address->labtab_len = 1;
	code = (CODEBUF_TYPE *)base_address->ptext_adr;	/* start of executable code */
#ifdef __ia64
	if (dyn_modes_type[mode_index][0] == 'C')
	{
		GEN_CALL_C(CODE_ADDRESS(dmode->func_ptr1))		/* line 0,1 */
	} else
	{
		GEN_CALL_ASM(CODE_ADDRESS(dmode->func_ptr1))		/* line 0,1 */
	}
#else
	GEN_CALL(dmode->func_ptr1);			/* line 0,1 */
#endif /* __ia64 */

#ifdef _AIX
	if (CI_MODE == mode_index)
	{
		/* Following 2 instructions are generated to call the routine stored in GTM_REG_ACCUM.
		 * ci_restart would have loaded this register with the address of op_extcall/op_extexfun.
		 * On other platforms, ci_start usually invokes op_ext* which will return directly
		 * to the generated code. Since RS6000 doesn't support call instruction without altering
		 * return address register (LR), the workaround is to call op_ext* not from ci_restart
		 * but from this dummy code
		 */
		*code++ = RS6000_INS_MTLR | GTM_REG_ACCUM << RS6000_SHIFT_RS;
		*code++ = RS6000_INS_BRL;
	}
#endif

#ifdef __ia64
        if (dyn_modes_type[mode_index][1] == 'C')
	{
                GEN_CALL_C(CODE_ADDRESS(dmode->func_ptr2))
	} else
	{
                GEN_CALL_ASM(CODE_ADDRESS(dmode->func_ptr2))
	}
#else
        GEN_CALL(dmode->func_ptr2);
#endif /* __ia64 */

#if defined (__ia64)
	if (DM_MODE == mode_index)
	{
		GEN_UNCOD_JUMP(-(2 * 5)); /* branch to dm_setup which is at the top of the direct mode frame. */
	}
#elif defined(__hpux)
	if (DM_MODE == mode_index)
	{
		*code++ = HPPA_INS_BEQ | (MAKE_COND_BRANCH_TARGET(-8) << HPPA_SHIFT_OFFSET); /* BEQ r0,r0, -8 */
		*code++ = HPPA_INS_NOP;
	}
#endif /* __ia64 */

#ifdef __ia64
        if (dyn_modes_type[mode_index][2] == 'C')
	{
                GEN_CALL_C(CODE_ADDRESS(dmode->func_ptr3));  	/* line 2 */
	} else
	{
                GEN_CALL_ASM(CODE_ADDRESS(dmode->func_ptr3));  /* line 2 */
	}
#else
        GEN_CALL(dmode->func_ptr3); 			/* line 2 */
#endif /* __ia64 */
	lnr = LNRTAB_ADR(base_address);
	*lnr++ = 0;								/* line 0 */
	*lnr++ = 0;								/* line 1 */
	IA64_ONLY(*lnr++ = 2 * CALL_SIZE + EXTRA_INST_SIZE;)			/* line 2 */
	NON_IA64_ONLY(*lnr++ = 2 * CALL_SIZE + EXTRA_INST * SIZEOF(int);)	/* line 2 */
	lbl = base_address->labtab_adr;
	lbl->lnr_adr = base_address->lnrtab_adr;
	base_address->current_rhead_adr = base_address;
	zlput_rname(base_address);
	inst_flush(base_address, algnd_rtnhdr_size + algnd_code_size + algnd_lbltab_size + algnd_lnrtab_size);
	return base_address;
}

#endif /* USHBIN_SUPPORTED */
