/****************************************************************
 *								*
 * Copyright (c) 2001-2023 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2017-2024 YottaDB LLC and/or its subsidiaries. *
 * All rights reserved.						*
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
#include "stack_frame.h"
#include "stringpool.h"
#include "svnames.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "tp.h"
#include "error.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#include "zshow.h"
#include "getstorage.h"
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
#include "ztimeout_routines.h"
#include "mlkdef.h"
#include "iormdef.h"
#include "toktyp.h"
#ifdef DEBUG
#  include "wbox_test_init.h"
#endif

#define ESC_OFFSET		4
#define MAX_COMMAND_LINE_LENGTH	255

GBLREF boolean_t		dollar_zquit_anyway;
GBLREF boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF int			process_exiting;
GBLREF int4			dollar_zeditor, dollar_zsystem, zdir_form;
GBLREF io_log_name		*dollar_principal, *io_root_log_name;
GBLREF io_pair			io_curr_device, io_std_device;
GBLREF mlk_subhash_val_t	mlk_last_hash;
GBLREF mstr			dollar_zchset, dollar_zicuver, dollar_zpatnumeric, dollar_zpin, dollar_zpout;
GBLREF mval			dollar_estack_delta, dollar_job, dollar_system, dollar_zdir, dollar_zerror, dollar_zgbldir;
GBLREF mval			dollar_zinterrupt, dollar_zproc, dollar_zsource, dollar_zstatus, dollar_ztexit, dollar_zyerror;
GBLREF mval			dollar_zcmdline;
GBLREF size_t			totalAlloc, totalAllocGta, totalRmalloc, totalRallocGta, totalUsed, totalUsedGta, zmalloclim;
GBLREF spdesc			stringpool;
GBLREF stack_frame		*frame_pointer;
GBLREF uint4			dollar_tlevel, dollar_trestart, dollar_zjob;
GBLREF volatile boolean_t	dollar_zininterrupt;

#ifdef GTM_TRIGGER
GBLREF	boolean_t		ztwormhole_used;		/* TRUE if $ztwormhole was used by trigger code */
GBLREF	int4			gtm_trigger_depth;
GBLREF	mstr			*dollar_ztname;
GBLREF	mval			*dollar_ztdata, *dollar_ztdelim, *dollar_ztoldval, *dollar_ztriggerop;
GBLREF	mval			dollar_ztslate, *dollar_ztupdate, *dollar_ztvalue, dollar_ztwormhole;
#endif

/***************** YottaDB-only GBLREFs *****************/
GBLREF mlk_subhash_val_t	mlk_last_hash;
GBLREF	int			jobinterrupt_sig_num;

#ifdef GTM_TRIGGER
GBLREF	boolean_t	write_ztworm_jnl_rec;
#endif

error_def(ERR_INVSVN);
error_def(ERR_UNIMPLOP);
error_def(ERR_ZDIROUTOFSYNC);

LITREF mval		literal_zero, literal_one, literal_null, literal_sqlnull;
LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;
LITREF char		ydb_release_stamp[];
LITREF int4		ydb_release_stamp_len;
LITREF char		ydb_release_name[];
LITREF int4		ydb_release_name_len;

void op_svget(int varnum, mval *v)
{
	char		*c1, *c2, director_token;
	d_rm_struct	*d_rm;
	gtm_uint64_t	ucount;
	int 		count;
	io_log_name	*tl;
	mval		*mvp;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
#	if defined(UNIX) && defined(DEBUG)
	if (ydb_white_box_test_case_enabled && (WBTEST_HUGE_ALLOC == ydb_white_box_test_case_number))
	{
		if (1 == ydb_white_box_test_case_count)
			totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed = totalUsedGta = 0xffff;
		else if (2 == ydb_white_box_test_case_count)
			totalAlloc = totalAllocGta = totalRmalloc = totalRallocGta = totalUsed = totalUsedGta = 0xfffffff;
		else if (3 == ydb_white_box_test_case_count)
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
			op_zhorolog(v, FALSE);
			break;
		case SV_ZHOROLOG:
			op_zhorolog(v, TRUE);
			break;
		case SV_ZUT:
			op_zut(v);
			break;
		case SV_ZGBLDIR:
			v->mvtype = MV_STR;
			v->str = dollar_zgbldir.str;
			break;
		case SV_ZPIN:
			/* if not a split device then ZPIN and ZPOUT will fall through to ZPRINCIPAL */
			if (io_std_device.in != io_std_device.out)
			{
				tl = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
				/* will define zpin as $p contents followed by "< /", for instance: /dev/tty4< / */
				ENSURE_STP_FREE_SPACE(tl->len + dollar_zpin.len);
				v->mvtype = MV_STR;
				v->str.addr = (char *)stringpool.free;
				/* first transfer $p */
				memcpy(stringpool.free, (char *)tl->dollar_io, tl->len);
				stringpool.free += tl->len;
				/* then transfer "< /" */
				memcpy(stringpool.free, dollar_zpin.addr, dollar_zpin.len);
				stringpool.free += dollar_zpin.len;
				v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
				break;
			}
		case SV_ZPOUT:
			/* if not a split device then ZPOUT will fall through to ZPRINCIPAL */
			if (io_std_device.in != io_std_device.out)
			{
				tl = dollar_principal ? dollar_principal : io_root_log_name->iod->trans_name;
				/* will define zpout as $p contents followed by "> /", for instance: /dev/tty4> / */
				ENSURE_STP_FREE_SPACE(tl->len + dollar_zpout.len);
				v->mvtype = MV_STR;
				v->str.addr = (char *)stringpool.free;
				/* first transfer $p */
				memcpy(stringpool.free, (char *)tl->dollar_io, tl->len);
				stringpool.free += tl->len;
				/* then transfer "< /" */
				memcpy(stringpool.free, dollar_zpout.addr, dollar_zpout.len);
				stringpool.free += dollar_zpout.len;
				v->str.len = INTCAST((char *)stringpool.free - v->str.addr);
				break;
			}
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
		case SV_ZAUDIT:
			MV_FORCE_MVAL(v, TREF(dollar_zaudit));
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
		case SV_ZCMDLINE:
			*v = dollar_zcmdline;
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
				gtm_putmsg_csa(CSA_ARG(NULL) VARLSTCNT(6) ERR_ZDIROUTOFSYNC, 4, v->str.len, v->str.addr,
					   dollar_zdir.str.len, dollar_zdir.str.addr);
			SKIP_DEVICE_IF_NOT_NEEDED(v);
			s2pool(&(v->str));
			break;
		case SV_ZSTEP:
			*v = TREF(dollar_zstep);
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
			/* If we are in the process of exiting and come here (e.g. to do ZSHOW dump as part of creating
			 * the fatal zshow dump file due to a fatal YDB-F-MEMORY error), do not invoke zro_init() as that
			 * might in turn require more memory (e.g. attach to relinkctl shared memory etc.) and we dont
			 * want to get a nested YDB-F-MEMORY error.
			 */
			if (!TREF(zro_root) && !process_exiting)
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
			v->str = (TREF(dollar_ztrap)).str;
			assert(!v->str.len || !ztrap_explicit_null);
			s2pool(&(v->str));
			break;
		case SV_DEVICE:
			get_dlr_device(v);
			break;
		case SV_KEY:
			get_dlr_key(v);
			break;
		case SV_ZRELDATE:
			v->mvtype = MV_STR;
			v->str.addr = (char *)ydb_release_stamp;
			v->str.len = ydb_release_stamp_len;
			break;
		case SV_ZVERSION:
			v->mvtype = MV_STR;
			v->str.addr = (char *)gtm_release_name;
			v->str.len = gtm_release_name_len;
			break;
		case SV_ZYINTRSIG:
			if (dollar_zininterrupt)
			{	/* At this point, there are only 2 signals that can trigger the $ZINTERRUPT mechanism.
				 * They are "SIGUSR1" or "SIGUSR2". Hence the below check. This will need to be revised if
				 * more signals are added to this list.
				 */
				v->mvtype = MV_STR;
				v->str.addr = ((SIGUSR1 == jobinterrupt_sig_num) ? "SIGUSR1" : "SIGUSR2");
				v->str.len = strlen(v->str.addr);
			} else
			{	/* We are not inside $ZINTERRUPT code. So this ISV should be set to "". */
				*v = literal_null;
			}
			break;
		case SV_ZYRELEASE:
			v->mvtype = MV_STR;
			v->str.addr = (char *)ydb_release_name;
			v->str.len = ydb_release_name_len;
			break;
		case SV_ZYSQLNULL:
			*v = literal_sqlnull;
			break;
		case SV_ZICUVER:
			v->mvtype = MV_STR;
			v->str = dollar_zicuver;
			break;
		case SV_ZSYSTEM:
			MV_FORCE_MVAL(v, dollar_zsystem);
			break;
		case SV_ZC:
		case SV_ZCSTATUS:
			/* Maintain the external $ZCSTATUS == 1 for SUCCESS on UNIX while internal good is 0 */
			MV_FORCE_MVAL(v, !TREF(dollar_zcstatus) ? 1 : TREF(dollar_zcstatus));
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
			v->str = (TREF(dollar_etrap)).str;
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTDELIM:
#			ifdef GTM_TRIGGER
			assert(!dollar_ztdelim || MV_DEFINED(dollar_ztdelim));
			if (NULL == dollar_ztdelim || !(MV_STR & dollar_ztdelim->mvtype) || (0 == dollar_ztdelim->str.len))
				memcpy(v, &literal_null, SIZEOF(mval));
			else
				memcpy(v, dollar_ztdelim, SIZEOF(mval));
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTRIGGEROP:
#			ifdef GTM_TRIGGER
			/* Value comes from GT.M, but assert it's a string */
			assert(!dollar_ztriggerop || (MV_STR & dollar_ztriggerop->mvtype));
			memcpy(v, (NULL != dollar_ztriggerop) ? dollar_ztriggerop : &literal_null, SIZEOF(mval));
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			write_ztworm_jnl_rec = TRUE;
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
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
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTLEVEL:
#			ifdef GTM_TRIGGER
			MV_FORCE_MVAL(v, gtm_trigger_depth);
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZONLNRLBK:
#			ifdef UNIX
			count = TREF(dollar_zonlnrlbk);
			MV_FORCE_MVAL(v, count);
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZCLOSE:
#			ifdef UNIX
			count = TREF(dollar_zclose);
			MV_FORCE_MVAL(v, count);
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZKEY:
			get_dlr_zkey(v);
			break;
		case SV_ZSTRPLLIM:
			count = stringpool.strpllim;
			MV_FORCE_MVAL(v, count);
			break;
		case SV_ZMALLOCLIM:
			MV_FORCE_UMVAL(v, zmalloclim);
			break;
		case SV_ZTIMEOUT:
			count = get_ztimeout(v);
			if (-1 == count)
				MV_FORCE_MVAL(v, count);
			break;
		case SV_ZMLKHASH:
			i2usmval(v, mlk_last_hash);
			break;
		default:
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_INVSVN);
	}
	if (!(MVTYPE_IS_SQLNULL(v->mvtype)))
	{
		if (!(MVTYPE_IS_STRING(v->mvtype)))
		{	/* in case op_svget is called at compile time; shouldn't hurt much any time */
			assert(MVTYPE_IS_NUMERIC(v->mvtype));
			n2s(v);
		} else if (!(MVTYPE_IS_NUMERIC(v->mvtype)))
		{	/* need to stop NUMOFLOW errors from preventing access to ISV values that s2n would flag as out of range */
			boolean_t	lcl_compile_time;

			assert(MVTYPE_IS_STRING(v->mvtype));
			lcl_compile_time = TREF(compile_time);
			director_token = TREF(director_token);
			TREF(director_token) = TK_STRLIT;
			TREF(compile_time) = TRUE;
			s2n(v);
			TREF(director_token) = director_token;
			TREF(compile_time) = lcl_compile_time;
		}
	}
}
