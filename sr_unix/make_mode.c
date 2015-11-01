/****************************************************************
 *								*
 *	Copyright 2001, 2003 Sanchez Computer Associates, Inc.	*
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
#include "rtnhdr.h"
#include "op.h"
#include "compiler.h"
#include "emit_code.h"
#include "gtmci.h"
#include "inst_flush.h"

#ifdef USHBIN_SUPPORTED
/* From here down is only defined in a shared binary environment */

#include "make_mode.h"


/* This routine is called to create (currently two different) dynamic routines that
   can be executed. One is a direct mode frame, the other is a callin base frame. They
   are basically identical except in the entry points that they call. To this end, this
   common routine is called for both with the entry points to be put into the generated
   code being passed in as parameters.
*/

typedef struct dyn_modes_struct
{
	char 		*rtn_name;
	int		rtn_name_len;
	void		(*func_ptr1)(void);
	void		(*func_ptr2)(void);
	void		(*func_ptr3)(void);
} dyn_modes;

static dyn_modes our_modes[2] =
{
	{
		GTM_DMOD,
		sizeof(GTM_DMOD) - 1,
		dm_setup,
		mum_tstart,
		opp_ret
	},
	{
		GTM_CIMOD,
		sizeof(GTM_CIMOD) - 1,
		ci_restart,
		ci_ret_code,
		opp_ret
	}
};


rhdtyp *make_mode (int mode_index)
{
	rhdtyp		*base_address;
	lab_tabent	*lbl;
	lnr_tabent	*lnr;
	unsigned int	*code;
	dyn_modes	*dmode;

	assert(DM_MODE == mode_index || CI_MODE == mode_index);
	base_address = (rhdtyp *)malloc(sizeof(rhdtyp) + CODE_SIZE + sizeof(lab_tabent) + CODE_LINES * sizeof(lnr_tabent));
	memset(base_address, 0, 	sizeof(rhdtyp) + CODE_SIZE + sizeof(lab_tabent) + CODE_LINES * sizeof(lnr_tabent));
	dmode = &our_modes[mode_index];
	memcpy(&base_address->routine_name, dmode->rtn_name, dmode->rtn_name_len);

	base_address->ptext_adr = (unsigned char *)base_address + sizeof(rhdtyp);
	base_address->ptext_end_adr = (unsigned char *)base_address->ptext_adr + CODE_SIZE;

	base_address->lnrtab_adr = (lnr_tabent *)base_address->ptext_end_adr;

	base_address->labtab_adr = (lab_tabent *)((unsigned char *)base_address + sizeof(rhdtyp) +
						  CODE_SIZE + CODE_LINES * sizeof(lnr_tabent));

	base_address->lnrtab_len = CODE_LINES;
	base_address->labtab_len = 1;

	code = (unsigned int *)base_address->ptext_adr;	/* start of executable code */
	GEN_CALL(dmode->func_ptr1);			/* line 0,1 */
#ifdef _AIX
	if (CI_MODE == mode_index)
	{
		/* Following 2 instructions are generated to call the routine stored in GTM_REG_ACCUM.
		 * ci_restart would have loaded this register with the address of op_extcall/op_extexfun.
		 * On other platforms, ci_start usually invokes op_ext* which will return directly
		 * to the generated code. Since RS6000 doesn't support call instruction without altering
		 * return address register (LR), the workaround is to call op_ext* not from ci_restart
		 * but from this dummy code */
		*code++ = RS6000_INS_MTLR | GTM_REG_ACCUM << RS6000_SHIFT_RS;
		*code++ = RS6000_INS_BRL;
	}
#endif
	GEN_CALL(dmode->func_ptr2);
#ifdef __hpux
	if (DM_MODE == mode_index)
	{
		*code++ = HPPA_INS_BEQ | (MAKE_COND_BRANCH_TARGET(-8) << HPPA_SHIFT_OFFSET); /* BEQ r0,r0, -8 */
		*code++ = HPPA_INS_NOP;
	}
#endif
	GEN_CALL(dmode->func_ptr3);				/* line 2 */

	lnr = LNRTAB_ADR(base_address);
	*lnr++ = 0;						/* line 0 */
	*lnr++ = 0;						/* line 1 */
	*lnr++ = 2 * CALL_SIZE + EXTRA_INST * sizeof(int);	/* line 2 */

	lbl = base_address->labtab_adr;
	lbl->lnr_adr = base_address->lnrtab_adr;

	base_address->current_rhead_adr = base_address;
	zlput_rname(base_address);

	inst_flush(base_address, sizeof(rhdtyp) + CODE_SIZE + sizeof(lab_tabent) + CODE_LINES * sizeof(lnr_tabent));

	return base_address;
}

#endif /* USHBIN_SUPPORTED */
