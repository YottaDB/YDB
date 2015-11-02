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

#include "gtm_stdlib.h"
#include "gtm_string.h"

#include "svnames.h"
#include "io.h"
#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "indir_enum.h"
#include "error.h"
#include "stringpool.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#ifdef VMS
#include <fab.h>		/* needed for dbgbldir_sysops.h */
#endif
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "gtmmsg.h"
#include "ztrap_save_ctxt.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "gtm_ctype.h"
#include "setzdir.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "getzdir.h"
#include "gtm_newintrinsic.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "jnl.h"
#include "tp_timeout.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#endif

GBLREF gv_key		*gv_currkey;
GBLREF gv_namehead	*gv_target;
GBLREF gd_addr		*gd_header;
GBLREF gd_binding	*gd_map;
GBLREF gd_binding	*gd_map_top;
GBLREF io_pair		io_curr_device;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zstatus;
GBLREF mval		dollar_zgbldir;
GBLREF mval		dollar_zstep;
GBLREF mval		dollar_zsource;
GBLREF int		ztrap_form;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_zerror;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_system;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF boolean_t	ztrap_new;
GBLREF stack_frame	*error_frame;
GBLREF boolean_t	ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF mval		dollar_ztexit;
GBLREF boolean_t	dollar_ztexit_bool;
GBLREF boolean_t	dollar_zquit_anyway;
GBLREF boolean_t	tp_timeout_deferred;
#ifdef GTM_TRIGGER
GBLREF mval		*dollar_ztvalue;
GBLREF boolean_t	*ztvalue_changed_ptr;
GBLREF mval		*dollar_ztriggerop;
GBLREF mval		dollar_ztwormhole;
GBLREF mval		dollar_ztslate;
GBLREF int4		gtm_trigger_depth;
GBLREF int4		tstart_trigger_depth;
GBLREF uint4		dollar_tlevel;
#endif

#ifdef GTM_TRIGGER
LITREF mval		gvtr_cmd_mval[GVTR_CMDTYPES];
#endif

#define SIZEOF_prombuf ggl_prombuf

error_def(ERR_INVECODEVAL);
error_def(ERR_NOZTRAPINTRIG);
error_def(ERR_SETECODE);
error_def(ERR_SETINSETTRIGONLY);
error_def(ERR_SETINTRIGONLY);
error_def(ERR_SYSTEMVALUE);
error_def(ERR_UNIMPLOP);
error_def(ERR_ZTWORMHOLE2BIG);

void op_svput(int varnum, mval *v)
{
	int	i, ok, state;
	char	*vptr;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch (varnum)
	{
		case SV_X:
			MV_FORCE_NUM(v);
			io_curr_device.out->dollar.x = (short)MV_FORCE_INT(v);
			if ((short)(io_curr_device.out->dollar.x) < 0)
				io_curr_device.out->dollar.x = 0;
			break;
		case SV_Y:
			MV_FORCE_NUM(v);
			io_curr_device.out->dollar.y = (short)MV_FORCE_INT(v);
			if ((short)(io_curr_device.out->dollar.y) < 0)
				io_curr_device.out->dollar.y = 0;
			break;
		case SV_ZCOMPILE:
			MV_FORCE_STR(v);
			if ((TREF(dollar_zcompile)).addr)
				free ((TREF(dollar_zcompile)).addr);
			(TREF(dollar_zcompile)).addr = (char *)malloc(v->str.len);
			memcpy((TREF(dollar_zcompile)).addr, v->str.addr, v->str.len);
			(TREF(dollar_zcompile)).len = v->str.len;
			break;
		case SV_ZSTEP:
			MV_FORCE_STR(v);
			op_commarg(v,indir_linetail);
			op_unwind();
			dollar_zstep = *v;
			break;
		case SV_ZGBLDIR:
			MV_FORCE_STR(v);
			if ((dollar_zgbldir.str.len != v->str.len)
			    || memcmp(dollar_zgbldir.str.addr, v->str.addr, dollar_zgbldir.str.len))
			{
				if (0 == v->str.len)
				{
					/* set $zgbldir="" */
					dpzgbini();
					gd_header = NULL;
				} else
				{
					gd_header = zgbldir(v);
					/* update the gd_map */
					SET_GD_MAP;
					dollar_zgbldir.str.len = v->str.len;
					dollar_zgbldir.str.addr = v->str.addr;
					s2pool(&dollar_zgbldir.str);
				}
				if (NULL != gv_currkey)
				{
					gv_currkey->base[0] = '\0';
					gv_currkey->prev = gv_currkey->end = 0;
				} else if (NULL != gd_header)
					gvinit();
				if (NULL != gv_target)
					gv_target->clue.end = 0;
			}
			break;
		case SV_ZMAXTPTIME:
			TREF(dollar_zmaxtptime) = mval2i(v);
			break;
		case SV_ZROUTINES:
			MV_FORCE_STR(v);
			/* The string(v) should be parsed and loaded before setting $zroutines
			 * to retain the old value in case errors occur while loading */
			zro_load(&v->str);
			if ((TREF(dollar_zroutines)).addr)
				free ((TREF(dollar_zroutines)).addr);
			(TREF(dollar_zroutines)).addr = (char *)malloc(v->str.len);
			memcpy((TREF(dollar_zroutines)).addr, v->str.addr, v->str.len);
			(TREF(dollar_zroutines)).len = v->str.len;
			break;
		case SV_ZSOURCE:
			MV_FORCE_STR(v);
			dollar_zsource.mvtype = MV_STR;
			dollar_zsource.str = v->str;
			break;
		case SV_ZTRAP:
#			ifdef GTM_TRIGGER
			if (0 < gtm_trigger_depth)
				rts_error(VARLSTCNT(1) ERR_NOZTRAPINTRIG);
#			endif
			MV_FORCE_STR(v);
			if (ztrap_new)
				op_newintrinsic(SV_ZTRAP);
			dollar_ztrap.mvtype = MV_STR;
			dollar_ztrap.str = v->str;
			/* Setting either $ZTRAP or $ETRAP to empty causes any current error trapping to be canceled */
			if (!v->str.len)
			{
				dollar_etrap.mvtype = MV_STR;
				dollar_etrap.str = v->str;
				ztrap_explicit_null = TRUE;
			} else /* Ensure that $ETRAP and $ZTRAP are not both active at the same time */
			{
				ztrap_explicit_null = FALSE;
				if (dollar_etrap.str.len > 0)
					gtm_newintrinsic(&dollar_etrap);
			}
			if (ztrap_form & ZTRAP_POP)
				ztrap_save_ctxt();
			if (tp_timeout_deferred && !dollar_zininterrupt)
				/* A tp timeout was deferred. Now that $ETRAP is no longer in effect and no job interrupt is in
				 * effect, the timeout need no longer be deferred and can be recognized.
				 */
				tptimeout_set(0);
			break;
		case SV_ZSTATUS:
			MV_FORCE_STR(v);
			dollar_zstatus.mvtype = MV_STR;
			dollar_zstatus.str = v->str;
			break;
		case SV_PROMPT:
			MV_FORCE_STR(v);
			MV_FORCE_LEN_STRICT(v); /* Ensure that direct mode prompt will not have BADCHARs,
						 * otherwise the BADCHAR error may fill up the filesystem
						 */
			if (v->str.len <= SIZEOF_prombuf)
				(TREF(gtmprompt)).len = v->str.len;
			else if (!gtm_utf8_mode)
				(TREF(gtmprompt)).len = SIZEOF_prombuf;
#			ifdef UNICODE_SUPPORTED
			else
			{
				UTF8_LEADING_BYTE(v->str.addr + SIZEOF_prombuf, v->str.addr, vptr);
				(TREF(gtmprompt)).len = INTCAST(vptr - v->str.addr);
			}
#			endif
			memcpy((TREF(gtmprompt)).addr, v->str.addr, (TREF(gtmprompt)).len);
			break;
		case SV_ECODE:
			MV_FORCE_STR(v);
			if (v->str.len)
			{
				/* Format must be like ,Mnnn,Mnnn,Zxxx,Uxxx,
				 * Mnnn are ANSI standard error codes
				 * Zxxx are implementation-specific codes
				 * Uxxx are end-user defined codes
				 * Note that there must be commas at the start and at the end
				 */
				for (state = 2, i = 0; (i < v->str.len) && (state <= 2); i++)
				{
					switch(state)
					{
						case 2: state = (v->str.addr[i] == ',') ? 1 : 101;
							break;
						case 1: state = ((v->str.addr[i] == 'M') ||
								 (v->str.addr[i] == 'U') ||
								 (v->str.addr[i] == 'Z')) ? 0 : 101;
							break;
						case 0: state = (v->str.addr[i] == ',') ? 1 : 0;
							break;
					}
				}
				/* The above check would pass strings like ","
				 * so double-check that there are at least three characters
				 * (starting comma, ending comma, and something in between)
				 */
				if ((state != 1) || (v->str.len < 3))
				{
					/* error, ecode = M101 */
					rts_error(VARLSTCNT(4) ERR_INVECODEVAL, 2, v->str.len, v->str.addr);
				}
			}
			if (v->str.len > 0)
			{
				ecode_add(&v->str);
				rts_error(VARLSTCNT(2) ERR_SETECODE, 0);
			} else
			{
				NULLIFY_DOLLAR_ECODE;	/* reset $ECODE related variables to correspond to $ECODE = NULL state */
				NULLIFY_ERROR_FRAME;	/* we are no more in error-handling mode */
				if (tp_timeout_deferred && !dollar_zininterrupt)
					/* A tp timeout was deferred. Now that we are clear of error handling and no job interrupt
					 * is in process, allow the timeout to be recognized.
					 */
					tptimeout_set(0);
			}
			break;
		case SV_ETRAP:
			MV_FORCE_STR(v);
			dollar_etrap.mvtype = MV_STR;
			dollar_etrap.str = v->str;
			/* Setting either $ZTRAP or $ETRAP to empty causes any current error trapping to be canceled */
			if (!v->str.len)
			{
				dollar_ztrap.mvtype = MV_STR;
				dollar_ztrap.str = v->str;
			} else if (dollar_ztrap.str.len > 0)
			{	/* Ensure that $ETRAP and $ZTRAP are not both active at the same time */
				assert(FALSE == ztrap_explicit_null);
				gtm_newintrinsic(&dollar_ztrap);
			}
			ztrap_explicit_null = FALSE;
			break;
		case SV_ZERROR:
			MV_FORCE_STR(v);
			dollar_zerror.mvtype = MV_STR;
			dollar_zerror.str = v->str;
			break;
		case SV_ZYERROR:
			MV_FORCE_STR(v);
			dollar_zyerror.mvtype = MV_STR;
			dollar_zyerror.str = v->str;
			break;
		case SV_SYSTEM:
			assert(FALSE);
			rts_error(VARLSTCNT(4) ERR_SYSTEMVALUE, 2, v->str.len, v->str.addr);
			break;
		case SV_ZDIR:
			setzdir(v, NULL); 	/* change directory to v */
			getzdir(); 		/* update dollar_zdir with current working directory */
			break;
		case SV_ZINTERRUPT:
			MV_FORCE_STR(v);
			dollar_zinterrupt.mvtype = MV_STR;
			dollar_zinterrupt.str = v->str;
			break;
		case SV_ZDATE_FORM:
			MV_FORCE_NUM(v);
			TREF(zdate_form) = (short)MV_FORCE_INT(v);
			break;
		case SV_ZTEXIT:
			MV_FORCE_STR(v);
			dollar_ztexit.mvtype = MV_STR;
			dollar_ztexit.str = v->str;
			/* Coercing $ZTEXIT to boolean at SET command is more efficient than coercing before each
			 * rethrow at TR/TRO. Since we want to maintain dollar_ztexit as a string, coercion should
			 * not be performed on dollar_ztext, but on a temporary (i.e. parameter v)
			 */
			dollar_ztexit_bool = MV_FORCE_BOOL(v);
			break;
		case SV_ZQUIT:
			dollar_zquit_anyway = MV_FORCE_BOOL(v);
			break;
		case SV_ZTVALUE:
#			ifdef GTM_TRIGGER
			assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
			if (!dollar_tlevel || (tstart_trigger_depth == gtm_trigger_depth))
				rts_error(VARLSTCNT(4) ERR_SETINTRIGONLY, 2, RTS_ERROR_TEXT("$ZTVALUE"));
			if (dollar_ztriggerop != &gvtr_cmd_mval[GVTR_CMDTYPE_SET])
				rts_error(VARLSTCNT(4) ERR_SETINSETTRIGONLY, 2, RTS_ERROR_TEXT("$ZTVALUE"));
			assert(0 < gtm_trigger_depth);
			memcpy(dollar_ztvalue, v, SIZEOF(mval));
			dollar_ztvalue->mvtype &= ~MV_ALIASCONT;	/* Make sure to shut off alias container flag on copy */
			assert(NULL != ztvalue_changed_ptr);
			*ztvalue_changed_ptr = TRUE;
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTWORMHOLE:
#			ifdef GTM_TRIGGER
			MV_FORCE_STR(v);
			/* See jnl.h for why MAX_ZTWORMHOLE_SIZE should be less than minimum alignsize */
			assert(MAX_ZTWORMHOLE_SIZE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));
			if (MAX_ZTWORMHOLE_SIZE < v->str.len)
				rts_error(VARLSTCNT(4) ERR_ZTWORMHOLE2BIG, 2, v->str.len, MAX_ZTWORMHOLE_SIZE);
			dollar_ztwormhole.mvtype = MV_STR;
			dollar_ztwormhole.str = v->str;
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTSLATE:
#			ifdef GTM_TRIGGER
			assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
			if (!dollar_tlevel || (tstart_trigger_depth == gtm_trigger_depth))
				rts_error(VARLSTCNT(4) ERR_SETINTRIGONLY, 2, RTS_ERROR_TEXT("$ZTSLATE"));
			assert(0 < gtm_trigger_depth);
			MV_FORCE_DEFINED(v);
			memcpy((char *)&dollar_ztslate, v, SIZEOF(mval));
			dollar_ztslate.mvtype &= ~MV_ALIASCONT;	/* Make sure to shut off alias container flag on copy */
			break;
#			else
			rts_error(VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		default:
			GTMASSERT;
	}
	return;
}
