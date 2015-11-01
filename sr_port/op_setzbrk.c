/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"

#include "cache.h"
#include "mdq.h"
#include "rtnhdr.h"
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

GBLREF z_records	zbrk_recs;
GBLREF mident		zlink_mname;
GBLREF stack_frame	frame_pointer;
GBLREF unsigned short	proc_act_type;
GBLREF int		cache_temp_cnt;

void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt)
	/* act == action associated with ZBREAK */
	/* cnt == perform break after this many passes */
{
	char		*cp, zbloc_buff[sizeof(mident) + 1 + 10 + 1 + sizeof(mident)], *zbloc_end;
	mident		rtn_name;
	mstr		*obj;
	mval		rtn_str;
	rhdtyp		*routine;
	zb_code		*addr, tmp_xf_code;
	int4		*line_offset_addr, *next_line_offset_addr, addr_off;
	zbrk_struct	*z_ptr;
	cache_entry	*csp;
	uint4		status;
	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLMODULE);
	error_def(ERR_NOZBRK);
	error_def(ERR_COMMENT);
	error_def(ERR_NOPLACE);
	error_def(ERR_ZBREAKFAIL);
	error_def(ERR_MEMORY);
	error_def(ERR_VMSMEMORY);

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
		zr_remove(NULL);
	else
	{
		flush_pio();
		if (NULL == (routine = find_rtn_hdr(&rtn->str)))
		{	/* must strip trailing nulls */
			rtn_str = *rtn;
			for (cp = rtn_str.str.addr + rtn_str.str.len;  (0 == *--cp) && (rtn_str.str.len > 0);  rtn_str.str.len--)
				;
			op_zlink(&rtn_str, NULL);
			routine = find_rtn_hdr(&rtn->str);
			if (NULL == routine)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->str.len, rtn->str.addr,
						ERR_ZLMODULE, 2, mid_len(&zlink_mname), &zlink_mname);
		}
		if (NULL == (line_offset_addr = find_line_addr(routine, &lab->str, offset)))
			dec_err(VARLSTCNT(1) ERR_NOPLACE);
		else if (CANCEL_ONE == cnt)		/* cancel zbreak */
		{
			addr = (zb_code *)LINE_NUMBER_ADDR(CURRENT_RHEAD_ADR(routine), line_offset_addr);
			addr = find_line_call(addr);
			if (NULL != (z_ptr = zr_find(&zbrk_recs, addr)))
				zr_put_free(&zbrk_recs, z_ptr);
			else
				dec_err(VARLSTCNT(1) ERR_NOZBRK);
		} else if (0 <= cnt)		/* set zbreak */
		{
#ifdef ZB_AT_COMMENT_INFO
			next_line_offset_addr = find_line_addr(routine, &lab->str, offset + 1);
			if (NULL != next_line_offset_addr && *next_line_offset_addr == *line_offset_addr)
			{ /* we don't recognize the case of last line comment 'coz that line generates LINESTART, RET code */
				dec_err(VARLSTCNT(1) ERR_COMMENT);
			}
#endif
			/* Force creation of new cache record we can steal with _nocache */
			op_commarg(act, indir_linetail_nocache);
			obj = cache_get(indir_linetail_nocache, &act->str);
			csp = ((ihdtyp *)(obj->addr))->indce;	/* Cache entry for this object code */
			if (csp->temp_elem)			/* Going to be released when unwound? */
				++csp->refcnt;			/* .. this will keep it around */
			op_unwind();
			addr = (zb_code *)LINE_NUMBER_ADDR(CURRENT_RHEAD_ADR(routine), line_offset_addr);
			addr = find_line_call(addr);
			if (NULL == (z_ptr = zr_find(&zbrk_recs, addr)))
			{
#ifdef USHBIN_SUPPORTED
					if (NULL != routine->shlib_handle && NULL == routine->shared_ptext_adr)
					{ /* setting a breakpoint in a shared routine, need to make a private copy */
						addr_off = (unsigned char *)addr - routine->ptext_adr;
						if (SS_NORMAL == (status = cre_private_code_copy(routine)))
							addr = (zb_code *)(routine->ptext_adr + addr_off);
						else
						{
							assert(UNIX_ONLY(ERR_MEMORY) VMS_ONLY(ERR_VMSMEMORY) == status);
							/* convert to label+offset^routine to be presented to the user */
							zbloc_end = rtnlaboff2entryref(zbloc_buff, &rtn->str, &lab->str, offset);
							rts_error(VARLSTCNT(4) ERR_ZBREAKFAIL, 2, zbloc_end - zbloc_buff,
									zbloc_buff);
						}
					}
#endif
				z_ptr = zr_get_free(&zbrk_recs, addr);
				NON_USHBIN_ONLY(fix_pages((unsigned char *)addr, (unsigned char *)addr));
				z_ptr->m_opcode = *addr; /* save for later restore while cancelling breakpoint */
				tmp_xf_code = (z_ptr->m_opcode & ZB_CODE_MASK) >> ZB_CODE_SHIFT;
				if (xf_linefetch * sizeof(int4) == tmp_xf_code)
					*addr = (*addr & (~ZB_CODE_MASK)) | ((xf_zbfetch * sizeof(int4)) << ZB_CODE_SHIFT);
				else if (xf_linestart * sizeof(int4) == tmp_xf_code)
					*addr = (*addr & (~ZB_CODE_MASK)) | ((xf_zbstart * sizeof(int4)) << ZB_CODE_SHIFT);
				else if (((xf_zbstart * sizeof(int4)) != tmp_xf_code)
					&& ((xf_zbfetch * sizeof(int4)) != tmp_xf_code))
					GTMASSERT;
				memset(&z_ptr->rtn, 0, sizeof(mident));
				memcpy(&z_ptr->rtn, rtn->str.addr, rtn->str.len);
				memset(&z_ptr->lab, 0, sizeof(mident));
				memcpy(&z_ptr->lab, lab->str.addr, lab->str.len);
				z_ptr->offset = offset;
				z_ptr->mpc = (zb_code *)((unsigned char *)addr - SIZEOF_LA);
				inst_flush(addr, sizeof(*addr));
			}
			/* We want the compiled object code to be disassociated from the indr cache queue but it must maintain
			   its relationship with (our pseudo) cache entry in the zbrk_struct to work properly with comp_indr.
			*/
			if (z_ptr->action)
			{
				if (z_ptr->action->refcnt)
				{	/* This frame is active. Mark it as temp so gets released when sf is unwound */
					z_ptr->action->temp_elem = TRUE;
					DBG_INCR_CNT(cache_temp_cnt);
					z_ptr->action = (cache_entry *)malloc(sizeof(cache_entry));
				} else if (z_ptr->action->obj.addr)
					free(z_ptr->action->obj.addr);
			} else
				z_ptr->action = (cache_entry *)malloc(sizeof(cache_entry));
			*z_ptr->action = *csp;			/* Make copy of cache entry */
			dqdel(csp, linkq);			/* Remove entry from hash queue it was on */
			if (csp->temp_elem)
			{	/* was a temp elem which must be released */
				assert(1 == csp->refcnt);
				z_ptr->action->temp_elem = FALSE;
				z_ptr->action->refcnt = 0;
				dqdel(csp, linktemp);
				free(csp);
			} else
			{	/* normal cache entry. Lobotomize it so don't have two entries pointing to same memory */
				assert(0 == csp->refcnt);
				memset(csp, 0, sizeof(*csp));
			}
			/* Make this cached object code point back to it's new "cache entry". */
			((ihdtyp *)(z_ptr->action->obj.addr))->indce = z_ptr->action;
			z_ptr->count = cnt;
			z_ptr->action->linkq.fl = 0;		/* So nobody tries to dequeue it */
			z_ptr->action->linktemp.fl = 0;
		} else
			GTMASSERT;
	}
}
