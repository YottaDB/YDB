/****************************************************************
 *								*
 * Copyright (c) 2001-2017 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_stdio.h"

#include "cache.h"
#include <rtnhdr.h>
#include "zbreak.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "cachectl.h"
#include "op.h"
#include "fix_pages.h"
#include "io.h"
#include "inst_flush.h"
#include "private_code_copy.h"
#include "iosp.h"
#include "gtm_text_alloc.h"
#include "srcline.h"
#include "compiler.h"
#include "min_max.h"
#include "dm_setup.h"
#include "restrict.h"
#ifdef GTM_TRIGGER
# include "gdsroot.h"			/* for gdsfhead.h */
# include "gdsbt.h"			/* for gdsfhead.h */
# include "gdsfhead.h"
# include "trigger_read_andor_locate.h"
# include "gtm_trigger_trc.h"
#else
# define DBGIFTRIGR(x)
# define DBGTRIGR(x)
# define DBGTRIGR_ONLY(x)
#endif

GBLREF z_records		zbrk_recs;
GBLREF mident_fixed		zlink_mname;
GBLREF stack_frame		*frame_pointer;
GBLREF unsigned short		proc_act_type;

error_def(ERR_COMMENT);
error_def(ERR_INVZBREAK);
error_def(ERR_MEMORY);
error_def(ERR_NOPLACE);
error_def(ERR_NOZBRK);
error_def(ERR_RESTRICTEDOP);
error_def(ERR_TRIGNAMENF);
error_def(ERR_VMSMEMORY);
error_def(ERR_ZBREAKFAIL);
error_def(ERR_ZLINKFILE);
error_def(ERR_ZLMODULE);

void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt)
	/* act == action associated with ZBREAK */
	/* cnt == perform break after this many passes */
{
	char		*cp, zbloc_buff[MAX_ENTRYREF_LEN], *zbloc_end;
	mident		*lab_name, *dummy;
	mident		rname, lname;
	mstr		*obj, tmprtnname;
	rhdtyp		*routine;
	zb_code		*addr, tmp_xf_code;
	int4		*line_offset_addr, *next_line_offset_addr;
	ssize_t		addr_off;
	zbrk_struct	*z_ptr;
	cache_entry	*csp;
	uint4		status;
	int		sstatus;
	icode_str	indir_src;
	boolean_t	deleted;
	GTMTRIG_ONLY(boolean_t	is_trigger);

	if (RESTRICTED(zbreak_op))
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "ZBREAK");

	MV_FORCE_STR(rtn);
	MV_FORCE_STR(lab);
	MV_FORCE_STR(act);
	if (NULL == zbrk_recs.beg)
	{
		assert(NULL == zbrk_recs.free);
		assert(NULL == zbrk_recs.end);
		zr_init(&zbrk_recs, INIT_NUM_ZBREAKS);
	}
	if (CANCEL_ALL == cnt)
		zr_remove_zbrks(NULL, NOBREAKMSG);
	else
	{
#		ifdef GTM_TRIGGER
		IS_TRIGGER_RTN(&rtn->str, is_trigger);
		if (is_trigger && (RESTRICTED(trigger_mod)))
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(3) ERR_RESTRICTEDOP, 1, "ZBREAK");
		DBGIFTRIGR((stderr, "op_setzbrk: Setting/clearing a zbreak in a trigger\n"));
#		endif
		flush_pio();
		if (WANT_CURRENT_RTN(rtn))
			routine = CURRENT_RHEAD_ADR(frame_pointer->rvector);
		else if (NULL == (routine = find_rtn_hdr(&rtn->str)))	/* Note assignment */
		{
#			ifdef GTM_TRIGGER
			/* trigger_source_read_andor_verify may alter the length part of the mstr to remove the +BREG
			 * region-name specification (the string component is unmodified). Pass in a copy of the mstr
			 * struct to avoid modification to routine->str as it affects the caller which relies on this
			 * variable being untouched.
			 */
			tmprtnname = rtn->str;
			if (is_trigger)
			{
				routine = NULL;				/* Init so garbage value isn't used */
				sstatus = trigger_source_read_andor_verify(&tmprtnname, &routine);
				if (0 != sstatus)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_TRIGNAMENF, 2, rtn->str.len, rtn->str.addr);
				assert(NULL != routine);
			} else
#			endif
			{
				op_zlink(rtn, NULL);
				routine = find_rtn_hdr(&rtn->str);
				if (NULL == routine)
					rts_error_csa(CSA_ARG(NULL) VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->str.len, rtn->str.addr,
						  ERR_ZLMODULE, 2, mid_len(&zlink_mname), &zlink_mname.c[0]);
			}
		}
		lab_name = NULL;
		if (NULL == (line_offset_addr = find_line_addr(routine, &lab->str, offset, &lab_name)))
			dec_err(VARLSTCNT(1) ERR_NOPLACE);
		else if (CANCEL_ONE == cnt)	/* Cancel ZBREAK */
		{
			addr = (zb_code *)LINE_NUMBER_ADDR(CURRENT_RHEAD_ADR(routine), line_offset_addr);
			addr = find_line_call(addr);
			if (NULL != (z_ptr = zr_find(&zbrk_recs, addr, RETURN_CLOSEST_MATCH_FALSE)))
				zr_remove_zbreak(&zbrk_recs, z_ptr);
			else
				dec_err(VARLSTCNT(1) ERR_NOZBRK);
		} else if (0 <= cnt)		/* Set ZBREAK */
		{
#			ifdef ZB_AT_COMMENT_INFO
			dummy = NULL;
			next_line_offset_addr = find_line_addr(routine, &lab->str, offset + 1, &dummy);
			if (NULL != next_line_offset_addr && *next_line_offset_addr == *line_offset_addr)
			{ 	/* We don't recognize the case of last line comment 'coz that line generates LINESTART, RET code */
				dec_err(VARLSTCNT(1) ERR_COMMENT);
				assert(lab_name == dummy);
			}
#			endif
			MEMVCMP(GTM_DMOD, (SIZEOF(GTM_DMOD) - 1), routine->routine_name.addr, routine->routine_name.len, sstatus);
			if (!sstatus)				/* sstatus == 0 meaning this is the GTM$DMOD routine - error out */
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(1) ERR_INVZBREAK);
			op_commarg(act, indir_linetail); 	/* This puts entry in stack and also increments refcnt field */
			indir_src.str = act->str;
			indir_src.code = indir_linetail;
			obj = cache_get(&indir_src);
			assert(NULL != obj);
			csp = ((ihdtyp *)(obj->addr))->indce;	/* Cache entry for this object code */
			csp->zb_refcnt++;			/* This will keep it around */
			op_unwind();				/* This removes entry from stack and decrements refcnt field */
			addr = (zb_code *)LINE_NUMBER_ADDR(CURRENT_RHEAD_ADR(routine), line_offset_addr);
			/* On HPPA (& other platforms) the addr returned is the address of the field in the instruction which is
			 * the offset of xfer table. But on IA64, as this field is set in the ADDS instruction as 14 bit immed
			 * value and this is written into 3 bitwise fields, we need to return the address of the whole instruction
			 * itself (in our case bundle as we are using a bundle for every instruction in the generated code.
			 */
			addr = find_line_call(addr);
			if (NULL == (z_ptr = zr_find(&zbrk_recs, addr, RETURN_CLOSEST_MATCH_FALSE)))
			{
#				ifdef USHBIN_SUPPORTED
				if ((NULL != routine->shared_ptext_adr) && (routine->shared_ptext_adr == routine->ptext_adr))
				{	/* Setting a breakpoint in a shared routine, need to make a private copy */
					addr_off = (unsigned char *)addr - routine->ptext_adr;
					if (SS_NORMAL == (status = cre_private_code_copy(routine)))
						addr = (zb_code *)(routine->ptext_adr + addr_off);
					else
					{
						assert(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY) == status);
						/* Convert to label+offset^routine to be presented to the user */
						rname.len = rtn->str.len;
						rname.addr = rtn->str.addr;
						lname.len = lab->str.len;
						lname.addr = lab->str.addr;
						zbloc_end = rtnlaboff2entryref(zbloc_buff, &rname, &lname, offset);
						rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_ZBREAKFAIL, 2, zbloc_end - zbloc_buff,
								zbloc_buff);
					}
				}
#				endif
				z_ptr = zr_add_zbreak(&zbrk_recs, addr);
				NON_USHBIN_ONLY(fix_pages((unsigned char *)addr, (unsigned char *)addr));
				/* Modify the instruction at the ZBREAK site. See zbreaksp.h for description of how this works
				 * on a given platform.
				 *
				 * Save original instruction for later restore when cancelling this breakpoint.
				 */
#				ifdef	COMPLEX_INSTRUCTION_UPDATE
				EXTRACT_OFFSET_TO_M_OPCODE(z_ptr->m_opcode, addr);
#				else
				z_ptr->m_opcode = *addr;
#				endif
				/* Modify op_linestart or op_linefetch transfer table reference instruction to the appropriate
				 * ZBREAK related flavor of instruction instead.
				 */
				tmp_xf_code = (z_ptr->m_opcode & ZB_CODE_MASK) >> ZB_CODE_SHIFT;
				if (xf_linefetch * SIZEOF(UINTPTR_T) == tmp_xf_code)
				{
#					ifdef	COMPLEX_INSTRUCTION_UPDATE
					FIX_OFFSET_WITH_ZBREAK_OFFSET(addr, xf_zbfetch);
#					else
					*addr = (*addr & (zb_code)(~ZB_CODE_MASK)) |
						((xf_zbfetch * SIZEOF(UINTPTR_T)) << ZB_CODE_SHIFT);
#					endif
				} else if (xf_linestart * SIZEOF(UINTPTR_T) == tmp_xf_code)
				{
#					ifdef	COMPLEX_INSTRUCTION_UPDATE
					FIX_OFFSET_WITH_ZBREAK_OFFSET(addr, xf_zbstart);
#					else
					*addr = (*addr & (zb_code)(~ZB_CODE_MASK)) |
						((xf_zbstart * SIZEOF(UINTPTR_T)) << ZB_CODE_SHIFT);
#					endif
				} else
					assertpro( ((xf_zbstart * SIZEOF(UINTPTR_T)) == tmp_xf_code)
						|| ((xf_zbfetch * SIZEOF(UINTPTR_T)) == tmp_xf_code));
				z_ptr->rtn = &(CURRENT_RHEAD_ADR(routine))->routine_name;
				assert(NULL != lab_name);
				z_ptr->lab = lab_name;
				z_ptr->offset = offset;
				z_ptr->mpc = (zb_code *)((unsigned char *)addr - SIZEOF_LA);
				z_ptr->rtnhdr = routine;
				USHBIN_ONLY(routine->has_ZBREAK = TRUE);	/* USHBIN platforms know which rtns have ZBREAK */
				inst_flush(addr, SIZEOF(INST_TYPE));
			}
			if (z_ptr->action)
			{	/* A ZBREAK command was already set for this line. Note when new action is same as
				 * old action, no resultant changes in zb_refcnt.
				 */
				assert(0 <z_ptr->action->zb_refcnt);
				z_ptr->action->zb_refcnt--;
				assert((z_ptr->action != csp) || (0 <z_ptr->action->zb_refcnt));
			}
			z_ptr->action = csp;
			z_ptr->count = cnt;
		} else
			assertpro(FALSE && line_offset_addr && cnt);
	}
}
