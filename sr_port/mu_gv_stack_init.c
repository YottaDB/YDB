/****************************************************************
 *								*
 *	Copyright 2003, 2007 Fidelity Information Services, Inc	*
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
#include "rtnhdr.h"
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

void mu_gv_stack_init(unsigned char **mstack_ptr)
{
	gv_currkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_altkey = (gv_key *)malloc(sizeof(gv_key) - 1 + gv_keysize);
	gv_currkey->top = gv_altkey->top = gv_keysize;
	gv_currkey->end = gv_currkey->prev = gv_altkey->end = gv_altkey->prev = 0;
	gv_altkey->base[0] = gv_currkey->base[0] = '\0';
	/* There may be M transactions in the journal files.  If so, op_tstart() and op_tcommit()
	   will be called during recovery;  they require a couple of dummy stack frames to be set up */
	*mstack_ptr = (unsigned char *)malloc(USER_STACK_SIZE);
	msp = stackbase = *mstack_ptr + USER_STACK_SIZE - sizeof(char *);
	mv_chain = (mv_stent *)msp;
	stacktop = *mstack_ptr + 2 * mvs_size[MVST_NTAB];
	stackwarn = stacktop + 1024;
	msp -= sizeof(stack_frame);
	frame_pointer = (stack_frame *)msp;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->type = SFT_COUNT;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer; /* no temporaries in this frame */
	--frame_pointer;
	memset(frame_pointer, 0, sizeof(stack_frame));
	frame_pointer->type = SFT_COUNT;
	frame_pointer->temps_ptr = (unsigned char *)frame_pointer; /* no temporaries in this frame either */
	frame_pointer->old_frame_pointer = (stack_frame *)msp;
	msp = (unsigned char *)frame_pointer;
}
