/****************************************************************
 *								*
 *	Copyright 2001, 2013 Fidelity Information Services, Inc	*
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
#include "gdsblk.h"
#include "gdskill.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "gdscc.h"
#include "io.h"
#include "iottdef.h"
#include "jnl.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "stringpool.h"
#include "svnames.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#include "getstorage.h"
#include "get_command_line.h"
#include "getzposition.h"
#include "getzprocess.h"
#include "get_ret_targ.h"
#include "mv_stent.h"
#include "dollar_zlevel.h"
#include "gtmmsg.h"
#include "error_trap.h"
#include "setzdir.h"
#include "get_reference.h"
#include "dollar_quit.h"
#ifdef UNIX
#  include "iormdef.h"
#  ifdef DEBUG
#    include "wbox_test_init.h"
#  endif
#endif

#define ESC_OFFSET		4
#define MAX_COMMAND_LINE_LENGTH	255

GBLREF mval		dollar_zproc;
GBLREF mval		dollar_zdir;
GBLREF stack_frame	*frame_pointer;
GBLREF mval		dollar_estack_delta;
GBLREF spdesc		stringpool;
GBLREF io_pair		io_curr_device;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_log_name	*dollar_principal;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zgbldir;
GBLREF mval		dollar_job;
GBLREF uint4		dollar_zjob;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zstep;
GBLREF mval		dollar_zsource;
GBLREF int4		dollar_zsystem;
GBLREF int4		dollar_zeditor;
GBLREF uint4		dollar_tlevel;
GBLREF uint4		dollar_trestart;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_zerror;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_system;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF int4		zdir_form;
GBLREF boolean_t	ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF mval		dollar_ztexit;
GBLREF size_t		totalAlloc;
GBLREF size_t		totalRmalloc;
GBLREF size_t		totalUsed;
GBLREF size_t		totalAllocGta;
GBLREF size_t		totalRallocGta;
GBLREF size_t		totalUsedGta;
GBLREF mstr		dollar_zchset;
GBLREF mstr		dollar_zpatnumeric;
GBLREF boolean_t	dollar_zquit_anyway;
#ifdef GTM_TRIGGER
GBLREF	mstr		*dollar_ztname;
GBLREF	mval		*dollar_ztdata;
GBLREF	mval		*dollar_ztoldval;
GBLREF	mval		*dollar_ztriggerop;
GBLREF	mval		dollar_ztslate;
GBLREF	mval		*dollar_ztupdate;
GBLREF	mval		*dollar_ztvalue;
GBLREF	mval		dollar_ztwormhole;
GBLREF	int4		gtm_trigger_depth;
GBLREF	boolean_t	ztwormhole_used;		/* TRUE if $ztwormhole was used by trigger code */
#endif

error_def(ERR_TEXT);
error_def(ERR_UNIMPLOP);
error_def(ERR_ZDIROUTOFSYNC);

LITREF mval		literal_zero, literal_one, literal_null;
LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

void op_svget(int varnum, mval *v)
{
	io_log_name	*tl;
	int 		count;
	gtm_uint64_t	ucount;
	char		*c1, *c2;
	mval		*mvp;
#	ifdef UNIX
	d_rm_struct	*d_rm;
#	endif
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	if defined(UNIX) && defined(DEBUG)
	if (gtm_white_box_test_case_enabled && (WBTEST_HUGE_ALLOC == gtm_white_box_test_case_number))
	{
		if (1 == gtm_white_box_test_case_count)
			totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed = totalUsedGta = 0xffff;
		else if (2 == gtm_white_box_test_case_count)
			totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed = totalUsedGta = 0xfffffff;
		else if (3 == gtm_white_box_test_case_count)
		{
#			ifdef GTM64
			if (8 == SIZEOF(size_t))
				totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta
					= totalUsed = totalUsedGta = 0xfffffffffffffff;
			else
#			endif
				totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed = totalUsedGta = 0xfffffff;
		} else
			totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed
				= totalUsedGta = GTM64_ONLY(SIZEOF(size_t)) NON_GTM64_ONLY(SIZEOF(size_t) > 4 ? 4 : SIZEOF(size_t));
	}
#	endif
	switch (varnum)
	{
		case SV_HOROLOG:
			op_horolog(v);
			break;
		case SV_ZGBLDIR:
			v->mvtype = MV_STR;
			v->str = dollar_zgbldir.str;
			break;
		case SV_PRINCIPAL:
			tl = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
			v->str.addr = tl->dollar_io;
			v->str.len = tl->len;
			/*** The following should be in the I/O code ***/
			if (ESC == *v->str.addr)
			{
				if (5 > v->str.len)
					v->str.len = 0;
				else
				{
					v->str.addr += ESC_OFFSET;
					v->str.len -= ESC_OFFSET;
				}
			}
			s2pool(&(v->str));
			v->mvtype = MV_STR;
			break;
		case SV_ZIO:
			v->mvtype = MV_STR;
			/* NOTE:	This is **NOT** equivalent to :
			 *		io_curr_log_name->dollar_io
			 */
			v->str.addr = io_curr_device.in->trans_name->dollar_io;
			v->str.len = io_curr_device.in->trans_name->len;
			if (ESC == *v->str.addr)
			{
				if (5 > v->str.len)
					v->str.len = 0;
				else
				{
					v->str.addr += ESC_OFFSET;
					v->str.len -= ESC_OFFSET;
				}
			}
			s2pool(&(v->str));
			break;
		case SV_JOB:
			*v = dollar_job;
			break;
		case SV_REFERENCE:
			get_reference(v);
			break;
		case SV_SYSTEM:
			*v = dollar_system;
			break;
		case SV_STORAGE:
			/* double2mval(v, getstorage()); Causes issues with unaligned stack on x86_64 - remove until fixed */
			i2mval(v, getstorage());
			break;
		case SV_TLEVEL:
			count = (int)dollar_tlevel;
			MV_FORCE_MVAL(v, count);
			break;
		case SV_TRESTART:
			MV_FORCE_MVAL(v, (int)((MAX_VISIBLE_TRESTART < dollar_trestart) ? MAX_VISIBLE_TRESTART : dollar_trestart));
			break;
		case SV_X:
			count = (int)io_curr_device.out->dollar.x;
			MV_FORCE_MVAL(v, count);
			break;
		case SV_Y:
			count = (int)io_curr_device.out->dollar.y;
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ZA:
			count = (int)io_curr_device.in->dollar.za;
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ZB:
			c1 = (char *)io_curr_device.in->dollar.zb;
			c2 = c1 + SIZEOF(io_curr_device.in->dollar.zb);
			ENSURE_STP_FREE_SPACE(SIZEOF(io_curr_device.in->dollar.zb));
			v->mvtype = MV_STR;
			v->str.addr = (char *)stringpool.free;
			while (c1 < c2 && *c1)
				*stringpool.free++ = *c1++;
			v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
			break;
		case SV_ZC:	/****THESE ARE DUMMY VALUES ONLY!!!!!!!!!!!!!!!!!****/
			MV_FORCE_MVAL(v, 0);
			break;
		case SV_ZCMDLINE:
			get_command_line(v, TRUE);	/* TRUE to indicate we want $ZCMDLINE
							   (i.e. processed not actual command line) */
			break;
		case SV_ZEOF:
#			ifdef UNIX
			if (rm == io_curr_device.in->type)
			{
				d_rm = (d_rm_struct *)io_curr_device.in->dev_sp;
				if (RM_READ != d_rm->lastop)
				{
					*v = literal_zero;
					break;
				}
			}
#			endif
			*v = io_curr_device.in->dollar.zeof ? literal_one : literal_zero;
			break;
		case SV_ZQUIT:
			*v = dollar_zquit_anyway ? literal_one : literal_zero;
			break;
		case SV_IO:
			v->str.addr = io_curr_device.in->name->dollar_io;
			v->str.len = io_curr_device.in->name->len;
			/*** The following should be in the I/O code ***/
			if (ESC == *v->str.addr)
			{
				if (5 > v->str.len)
					v->str.len = 0;
				else
				{
					v->str.addr += ESC_OFFSET;
					v->str.len -= ESC_OFFSET;
				}
			}
			s2pool(&(v->str));
			v->mvtype = MV_STR;
			break;
		case SV_PROMPT:
			v->mvtype = MV_STR;
			v->str.addr = (TREF(gtmprompt)).addr;
			v->str.len = (TREF(gtmprompt)).len;
			s2pool(&v->str);
			break;
		case SV_ZCOMPILE:
			v->mvtype = MV_STR;
			v->str = TREF(dollar_zcompile);
			s2pool(&(v->str));
			break;
		case SV_ZDIR:
			setzdir(NULL, v);
			if (v->str.len != dollar_zdir.str.len || 0 != memcmp(v->str.addr, dollar_zdir.str.addr, v->str.len))
				gtm_putmsg(VARLSTCNT(6) ERR_ZDIROUTOFSYNC, 4, v->str.len, v->str.addr,
					   dollar_zdir.str.len, dollar_zdir.str.addr);
			SKIP_DEVICE_IF_NOT_NEEDED(v);
			s2pool(&(v->str));
			break;
		case SV_ZSTEP:
			*v = dollar_zstep;
			break;
		case SV_ZMODE:
			*v = TREF(dollar_zmode);
			break;
		case SV_ZMAXTPTIME:
			i2mval(v, TREF(dollar_zmaxtptime));
			break;
		case SV_ZPOS:
			getzposition(v);
			break;
		case SV_ZPROC:
			getzprocess();
			*v = dollar_zproc;
			break;
		case SV_ZLEVEL:
			count = dollar_zlevel();
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ZROUTINES:
			if (!TREF(zro_root))
				zro_init();
			v->mvtype = MV_STR;
			v->str = TREF(dollar_zroutines);
			s2pool(&(v->str));
			break;
		case SV_ZSOURCE:
			v->mvtype = MV_STR;
			v->str = dollar_zsource.str;
			break;
		case SV_ZSTATUS:
			*v = dollar_zstatus;
			s2pool(&(v->str));
			break;
		case SV_ZTRAP:
			v->mvtype = MV_STR;
			v->str = dollar_ztrap.str;
			assert(!v->str.len || !ztrap_explicit_null);
			s2pool(&(v->str));
			break;
		case SV_DEVICE:
			get_dlr_device(v);
			break;
		case SV_KEY:
			get_dlr_key(v);
			break;
		case SV_ZVERSION:
			v->mvtype = MV_STR;
			v->str.addr = (char *)gtm_release_name;
			v->str.len = gtm_release_name_len;
			break;
		case SV_ZSYSTEM:
			MV_FORCE_MVAL(v, dollar_zsystem);
			break;
		case SV_ZCSTATUS:
			/* Maintain the external $ZCSTATUS == 1 for SUCCESS on UNIX while internal good is 0 */
			MV_FORCE_MVAL(v, UNIX_ONLY((0 == TREF(dollar_zcstatus)) ? 1 : ) TREF(dollar_zcstatus));
			break;
		case SV_ZEDITOR:
			MV_FORCE_MVAL(v, dollar_zeditor);
			break;
		case SV_QUIT:
			MV_FORCE_MVAL(v, dollar_quit());
			break;
		case SV_ECODE:
			ecode_get(-1, v);
			break;
		case SV_ESTACK:
			count = (dollar_zlevel() - 1) - dollar_estack_delta.m[0];
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ETRAP:
			v->mvtype = MV_STR;
			v->str = dollar_etrap.str;
			assert(!v->str.len || !ztrap_explicit_null);
			s2pool(&(v->str));
			break;
		case SV_STACK:
			count = (dollar_zlevel() - 1);
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ZERROR:
			v->mvtype = MV_STR;
			v->str = dollar_zerror.str;
			s2pool(&(v->str));
			break;
		case SV_ZYERROR:
			v->mvtype = MV_STR;
			v->str = dollar_zyerror.str;
			s2pool(&(v->str));
			break;
		case SV_ZINTERRUPT:
			v->mvtype = MV_STR;
			v->str = dollar_zinterrupt.str;
			s2pool(&(v->str));
			break;
		case SV_ZININTERRUPT:
			MV_FORCE_MVAL(v, dollar_zininterrupt);
			break;
		case SV_ZJOB:
			MV_FORCE_UMVAL(v, dollar_zjob);
			break;
		case SV_ZDATE_FORM:
			MV_FORCE_MVAL(v, TREF(zdate_form));
			break;
		case SV_ZTEXIT:
			*v = dollar_ztexit;
			break;
		case SV_ZALLOCSTOR:
			ucount = (gtm_uint64_t)totalAlloc + (gtm_uint64_t)totalAllocGta;
			ui82mval(v, ucount);
			break;
		case SV_ZREALSTOR:
			ucount = (gtm_uint64_t)totalRmalloc + (gtm_uint64_t)totalRallocGta;
			ui82mval(v, ucount);
			break;
		case SV_ZUSEDSTOR:
			ucount = (gtm_uint64_t)totalUsed + (gtm_uint64_t)totalUsedGta;
			ui82mval(v, ucount);
			break;
		case SV_ZCHSET:
			v->mvtype = MV_STR;
			v->str = dollar_zchset;
			break;
		case SV_ZPATNUMERIC:
			v->mvtype = MV_STR;
			v->str = dollar_zpatnumeric;
			break;
		case SV_ZTNAME:
		case SV_ZTCODE:		/* deprecated */
#			ifdef GTM_TRIGGER
			if (NULL == dollar_ztname)
				memcpy(v, &literal_null, SIZEOF(mval));
			else
			{
				v->mvtype = MV_STR;
				v->str.addr = dollar_ztname->addr;
				v->str.len = dollar_ztname->len;
			}
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTDATA:
#			ifdef GTM_TRIGGER
			/* Value comes from GT.M, but it might be numeric and need conversion to a string */
			assert(!dollar_ztdata || MV_DEFINED(dollar_ztdata));
			if (NULL != dollar_ztdata)
				MV_FORCE_STR(dollar_ztdata);
			memcpy(v, (NULL != dollar_ztdata) ? dollar_ztdata : &literal_null, SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTOLDVAL:
#			ifdef GTM_TRIGGER
			/* Value comes from GT.M, but it might be numeric and need conversion to a string */
			assert(!dollar_ztoldval || MV_DEFINED(dollar_ztoldval));
			if (NULL != dollar_ztoldval)
				MV_FORCE_STR(dollar_ztoldval);
			memcpy(v, (NULL != dollar_ztoldval) ? dollar_ztoldval : &literal_null, SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTRIGGEROP:
#			ifdef GTM_TRIGGER
			/* Value comes from GT.M, but assert it's a string */
			assert(!dollar_ztriggerop || (MV_STR & dollar_ztriggerop->mvtype));
			memcpy(v, (NULL != dollar_ztriggerop) ? dollar_ztriggerop : &literal_null, SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTUPDATE:
#			ifdef GTM_TRIGGER
			/* Value comes from GT.M, but if there were no delims involved, the value will be undefined, and
			 * we return a "literal_null".
			 */
			memcpy(v, ((NULL != dollar_ztupdate && (MV_STR & dollar_ztupdate->mvtype)) ? dollar_ztupdate
				   : &literal_null), SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTVALUE:
#			ifdef GTM_TRIGGER
			/* Value comes from user-land so make sure things are proper */
			assert(!dollar_ztvalue || MV_DEFINED(dollar_ztvalue));
			if (NULL != dollar_ztvalue)
				MV_FORCE_STR(dollar_ztvalue);
			memcpy(v, (NULL != dollar_ztvalue) ? dollar_ztvalue : &literal_null, SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTWORMHOLE:
#			ifdef GTM_TRIGGER
			/* Value comes from user-land so make sure things are proper */
			mvp = &dollar_ztwormhole;
			if (MV_DEFINED(mvp))
			{
				MV_FORCE_STR(mvp);
				memcpy(v, mvp, SIZEOF(mval));
			} else
				memcpy(v, &literal_null, SIZEOF(mval));
			ztwormhole_used = TRUE;
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTSLATE:
#			ifdef GTM_TRIGGER
			/* Value comes from user-land so make sure things are proper */
			assert(MV_DEFINED((&dollar_ztslate)));
			mvp = &dollar_ztslate;
			MV_FORCE_STR(mvp);
			memcpy(v, mvp, SIZEOF(mval));
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTLEVEL:
#			ifdef GTM_TRIGGER
			MV_FORCE_MVAL(v, gtm_trigger_depth);
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZONLNRLBK:
#			ifdef UNIX
			count = TREF(dollar_zonlnrlbk);
			MV_FORCE_MVAL(v, count);
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		default:
			GTMASSERT;
	}
}
