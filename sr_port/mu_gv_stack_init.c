/****************************************************************
 *								*
 *	Copyright 2003, 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "gdsroot.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include <rtnhdr.h>
#include "mv_stent.h"
#include "stack_frame.h"
#include "mu_gv_stack_init.h"

GBLREF	gv_key			*gv_currkey;
GBLREF	gv_namehead		*gv_target;
GBLREF  gd_region               *gv_cur_region;
GBLREF  int4			gv_keysize;
GBLREF  gv_key                  *gv_altkey;
GBLREF  mv_stent                *mv_chain;
GBLREF  stack_frame             *frame_pointer;
GBLREF  unsigned char           *msp, *stackbase, *stacktop, *stackwarn;

void mu_gv_stack_init(void)
{
	rhdtyp		*rtnhdr;
	lab_tabent	*labtbe;
	lnr_tabent	*lnrtbe;
	unsigned char	*mstack_ptr;

	GVKEY_INIT(gv_currkey, gv_keysize);
	GVKEY_INIT(gv_altkey, gv_keysize);
	/* There may be M transactions in the journal files.  If so, op_tstart() and op_tcommit()
	 * will be called during recovery;  they require a couple of dummy stack frames to be set up.
	 */
#	ifdef UNIX
	/* Triggers and error recovery in same will also need them. We don't actually need an executable routine,
	 * just one that "looks" executable for errorhandling.
	 */
	rtnhdr = (rhdtyp *)malloc(SIZEOF(rhdtyp) + RLEN + SIZEOF(lab_tabent) + (2 * SIZEOF(lnr_tabent)));
	memset(rtnhdr, 0, SIZEOF(rhdtyp));
	rtnhdr->src_full_name.addr = rtnhdr->routine_name.addr = UTIL_BASE_FRAME_NAME;
	rtnhdr->src_full_name.len = rtnhdr->routine_name.len = STRLEN(UTIL_BASE_FRAME_NAME);
	rtnhdr->lnrtab_len = 2;		/* 2 line number table entries (lines 0 and 1) */
#	ifdef USHBIN_SUPPORTED
	rtnhdr->ptext_adr = (unsigned char *)rtnhdr + SIZEOF(rhdtyp);
	rtnhdr->ptext_end_adr = rtnhdr->ptext_adr + RLEN;
	rtnhdr->vartab_adr = (var_tabent *)rtnhdr->ptext_end_adr;	/* No vars */
	rtnhdr->labtab_adr = (lab_tabent *)rtnhdr->ptext_end_adr;	/* 1 label */
	rtnhdr->lnrtab_adr = (lnr_tabent *)((char *)rtnhdr->labtab_adr + SIZEOF(lab_tabent));
	rtnhdr->current_rhead_adr = rtnhdr;
	labtbe = LABTAB_ADR(rtnhdr);
	labtbe->lnr_adr = LNRTAB_ADR(rtnhdr);
#	else	/* non-USHBIN */
	rtnhdr->ptext_off = SIZEOF(rhdtyp);
	rtnhdr->vartab_off = rtnhdr->ptext_off + RLEN;
	rtnhdr->labtab_off = rtnhdr->vartab_off;
	rtnhdr->lnrtab_off = rtnhdr->labtab_off + SIZEOF(lab_tabent);
	labtbe = LABTAB_ADR(rtnhdr);
	labtbe->lab_ln_ptr = 0;
	/* current_rhead_off can stay at its initialized 0 since it is an offset in non-USHBIN */
#	endif
	labtbe->lab_name.addr = UTIL_BASE_LABEL_NAME;
	labtbe->lab_name.len = STRLEN(UTIL_BASE_LABEL_NAME);
	lnrtbe = LNRTAB_ADR(rtnhdr);
	*lnrtbe++ = 0 NON_USHBIN_ONLY( + SIZEOF(*rtnhdr));
	*lnrtbe++ = 0 NON_USHBIN_ONLY( + SIZEOF(*rtnhdr));
	/* Create the base (back stop) frame */
	base_frame(rtnhdr);
	/* Now create the fake execution frame */
	new_stack_frame(rtnhdr, NULL, PTEXT_ADR(rtnhdr));
#	elif defined(VMS)
	mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
	msp = stackbase = mstack_ptr + USER_STACK_SIZE - SIZEOF(char *);
	mv_chain = (mv_stent *)msp;
	stacktop = mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + 1024;
	msp -= SIZEOF(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, SIZEOF(stack_frame));
	frame_pointer->type = SFT_COUNT;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer; /* no temporaries in this frame */
	--frame_pointer;
	memset(frame_pointer, 0, SIZEOF(stack_frame));
	frame_pointer->type = SFT_COUNT;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer; /* no temporaries in this frame either */
	frame_pointer->old_frame_pointer = (stack_frame *)msp;
	msp = (unsigned char *)frame_pointer;
#	else
#	  error "Unsupported platform"
#	endif
}
