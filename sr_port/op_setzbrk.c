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

#include "cache.h"
#include "mdq.h"
#include "zbreak.h"
#include "rtnhdr.h"
#include "stack_frame.h"
#include "xfer_enum.h"
#include "indir_enum.h"
#include "cachectl.h"
#include "op.h"
#include "fix_pages.h"
#include "io.h"
#include "inst_flush.h"
#include "gtm_string.h"

#ifdef __MVS__		/* need to adjust for load address inst. (temporary) */
#define SIZEOF_LA	4
#else
#define SIZEOF_LA	0
#endif

GBLREF z_records	zbrk_recs;
GBLREF mident		zlink_mname;
GBLREF stack_frame	frame_pointer;
GBLREF unsigned char	proc_act_type;

void	op_setzbrk(mval *rtn, mval *lab, int offset, mval *act, int cnt)
	/* act == action associated with ZBREAK */
	/* cnt == perform break after this many passes */
{
	char		*cp;
	mstr		*obj;
	mval		rtn_str;
	rhdtyp		*routine;
	zb_code		*addr, tmp_xf_code;
	int4		*line_offset_addr;
	zbrk_struct	*z_ptr;
	cache_entry	*csp;
	error_def(ERR_ZLINKFILE);
	error_def(ERR_ZLMODULE);
	error_def(ERR_NOZBRK);
	error_def(ERR_COMMENT);
	error_def(ERR_NOPLACE);

	MV_FORCE_STR(rtn);
	MV_FORCE_STR(lab);
	MV_FORCE_STR(act);
	if (0 == zbrk_recs.beg)
	{
		assert(0 == zbrk_recs.free);
		assert(0 == zbrk_recs.end);
		zr_init(&zbrk_recs, INIT_NUM_ZBREAKS, sizeof(zbrk_struct));
	}
	if (CANCEL_ALL == cnt)
	{
		while (z_ptr = (zbrk_struct *)zr_get_last(&zbrk_recs))
		{
			if (z_ptr->action)
			{
				if (z_ptr->action->obj.len)
					free(z_ptr->action->obj.addr);
				free(z_ptr->action);
			}
			addr = (zb_code *)(z_ptr->mpc);
			*addr = z_ptr->m_opcode;
			inst_flush(addr, sizeof(*addr));
			zr_put_free(&zbrk_recs, (char *)z_ptr);
		}
	} else
	{
		flush_pio();
		if (!(routine = find_rtn_hdr(&rtn->str)))
		{	/* must strip trailing nulls */
			rtn_str = *rtn;
			for (cp = rtn_str.str.addr + rtn_str.str.len;  (0 == *--cp) && (rtn_str.str.len > 0);  rtn_str.str.len--)
				;
			op_zlink(&rtn_str, 0);
			routine = find_rtn_hdr(&rtn->str);
			if (!routine)
				rts_error(VARLSTCNT(8) ERR_ZLINKFILE, 2, rtn->str.len, rtn->str.addr,
					ERR_ZLMODULE, 2, mid_len(&zlink_mname), &zlink_mname);
		}
		if (0 == (line_offset_addr = find_line_addr(routine, &lab->str, offset)))
			dec_err(VARLSTCNT(1) ERR_NOPLACE);
		else  if (CANCEL_ONE == cnt)		/* cancel zbreak */
		{
			addr = (zb_code *)(*line_offset_addr + routine->current_rhead_ptr + (char *)routine);
			addr = find_line_call(addr);
			if (0 != (z_ptr = (zbrk_struct *)zr_find(&zbrk_recs, (char *)addr)))
			{
				assert((zb_code *)(z_ptr->mpc) == addr);
				*addr = z_ptr->m_opcode;
				inst_flush(addr, sizeof(*addr));
				zr_put_free(&zbrk_recs, (char *)z_ptr);
			} else
				dec_err(VARLSTCNT(1) ERR_NOZBRK);
		} else  if (cnt >= 0)		/* set zbreak */
		{
			if (find_line_addr(routine, &lab->str, offset + 1) == line_offset_addr)
				dec_err(VARLSTCNT(1) ERR_COMMENT);
			proc_act_type = SFT_ZBRK_ACT;
			/* Force creation of new cache record we can steal with _nocache */
			op_commarg(act, indir_linetail_nocache);
			proc_act_type = 0;
			obj = cache_get(indir_linetail_nocache, &act->str);
			csp = ((ihdtyp *)(obj->addr))->indce;	/* Cache entry for this object code */
			if (csp->temp_elem)			/* Going to be released when unwound? */
				++csp->refcnt;			/* .. this will keep it around */
			op_unwind();
			addr = (zb_code *)(*line_offset_addr + routine->current_rhead_ptr + (char *)routine);
			addr = find_line_call(addr);
			if (0 == (z_ptr = (zbrk_struct *)zr_find(&zbrk_recs, (char *)addr)))
			{
				z_ptr = (zbrk_struct *)zr_get_free(&zbrk_recs, (char *)addr);
				fix_pages((unsigned char *)addr, (unsigned char *)addr);
				z_ptr->m_opcode = *addr & ZB_CODE_MASK;
				tmp_xf_code = z_ptr->m_opcode >> ZB_CODE_SHIFT;
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
				z_ptr->mpc = (char *)(addr) - SIZEOF_LA;
				inst_flush(addr, sizeof(*addr));
			}
			/* We want the compiled object code to be disassociated from the indr cache queue but it must maintain
			   its relationship with (our pseudo) cache entry in the zbrk_struct to work properly with comp_indr.
			*/
			if (z_ptr->action)
			{
				if (z_ptr->action->obj.addr)
					free(z_ptr->action->obj.addr);
			} else
				z_ptr->action = (cache_entry *)malloc(sizeof(cache_entry));
			*z_ptr->action = *csp;			/* Make copy of cache entry */
			dqdel(csp, linkq);
			if (csp->temp_elem)
			{	/* was a temp elem which must be released */
				assert(1 == csp->refcnt);
				z_ptr->action->temp_elem = FALSE;
				dqdel(csp, linktemp);
				free(csp);			/* Remove entry from hash queue it was on */
			} else
			{	/* normal cache entry. Lobotomize it so don't have two entries pointing to same memory */
				assert(0 == csp->refcnt);
				memset(csp, 0, sizeof(*csp));
			}
			/* Make this cached object code point back to it's new "cache entry". */
			((ihdtyp *)(z_ptr->action->obj.addr))->indce = z_ptr->action;	/* Set backward link to this cache entry */
			z_ptr->count = cnt;
		} else
			GTMASSERT;
	}
}
