/****************************************************************
 *								*
 * Copyright (c) 2001-2022 Fidelity National Information	*
 * Services, Inc. and/or its subsidiaries. All rights reserved.	*
 *								*
 * Copyright (c) 2018-2023 YottaDB LLC and/or its subsidiaries.	*
 * All rights reserved.						*
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
#include "dpgbldir.h"
#include "dpgbldir_sysops.h"
#include "gtmmsg.h"
#include "ztrap_save_ctxt.h"
#include "dollar_zlevel.h"
#include "error_trap.h"
#include "gtm_ctype.h"
#include "setzdir.h"
#include "stack_frame.h"
#include "getzdir.h"
#include "gtm_newintrinsic.h"
#include "filestruct.h"		/* needed for jnl.h */
#include "jnl.h"
#include "tp_timeout.h"
#include "stp_parms.h"
#ifdef GTM_TRIGGER
#include "gv_trigger.h"
#endif
#include "ztimeout_routines.h"
#include "have_crit.h"
#include "deferred_events.h"
#include "deferred_events_queue.h"
<<<<<<< HEAD
#include "util.h"
#include "ydb_logicals.h"
#include "ydb_setenv.h"
=======
#include "gtm_malloc.h"
#include "getstorage.h"
#include "try_event_pop.h"
>>>>>>> eb3ea98c (GT.M V7.0-002)


GBLREF boolean_t		dollar_zquit_anyway, dollar_ztexit_bool, malloccrit_issued, ztrap_new;
GBLREF boolean_t		ztrap_explicit_null;		/* whether $ZTRAP was explicitly set to NULL in this frame */
GBLREF gd_addr			*gd_header;
GBLREF gv_key			*gv_currkey;
GBLREF gv_namehead		*gv_target;
GBLREF io_pair			io_curr_device;
GBLREF mval			dollar_system, dollar_system_initial, dollar_zgbldir, dollar_zerror, dollar_zinterrupt;
GBLREF mval			dollar_zsource, dollar_zstatus, dollar_ztexit, dollar_zyerror, dollar_zcmdline;
GBLREF stack_frame		*error_frame;
GBLREF volatile int4		outofband;
GBLREF volatile boolean_t	dollar_zininterrupt;
GBLREF	size_t			totalRmalloc, totalRallocGta, zmalloclim;
#ifdef GTM_TRIGGER
GBLREF mval		*dollar_ztvalue;
GBLREF boolean_t	*ztvalue_changed_ptr;
GBLREF mval		*dollar_ztriggerop;
GBLREF mval		dollar_ztwormhole;
GBLREF mval		dollar_ztslate;
GBLREF int4		gtm_trigger_depth;
GBLREF int4		tstart_trigger_depth;
GBLREF uint4		dollar_tlevel;
GBLREF boolean_t	write_ztworm_jnl_rec;
#endif
GBLREF	dollar_ecode_type	dollar_ecode;			/* structure containing $ECODE related information */

LITREF mval		default_etrap;
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
<<<<<<< HEAD
	char	*vptr, lcl_str[256], *tmp;
	int	i, ok, state;
	mval	lcl_mval;
=======
	char	*vptr;
	int	i, ok, state, tmp;
>>>>>>> eb3ea98c (GT.M V7.0-002)
	int4	previous_gtm_strpllim;
	size_t	rtmp;
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
			TREF(dollar_zstep) = *v;
			break;
		case SV_ZGBLDIR:
			MV_FORCE_STR(v);
			if ((dollar_zgbldir.str.len != v->str.len)
				|| memcmp(dollar_zgbldir.str.addr, v->str.addr, dollar_zgbldir.str.len))
			{
				mval	ydb_cur_gbldir;

				if (0 == v->str.len)
				{
					dpzgbini();	/* Open default gbldir (i.e. SET $ZGBLDIR="") */
				} else
				{
					gd_header = zgbldir(v);
					dollar_zgbldir.str.len = v->str.len;
					dollar_zgbldir.str.addr = v->str.addr;
					s2pool(&dollar_zgbldir.str);
				}
				/* Set "ydb_cur_gbldir" env var to current $zgbldir. Utilities like %YDBPROCSTUCKEXEC look at this
				 * env var to know the current $zgbldir instead of what $ydb_gbldir env var points to (YDB#941).
				 */
				ydb_cur_gbldir.mvtype = MV_STR;
				ydb_cur_gbldir.str.addr = (char *)(ydbenvname[YDBENVINDX_CUR_GBLDIR] + 1);
				ydb_cur_gbldir.str.len = STRLEN(ydbenvname[YDBENVINDX_CUR_GBLDIR]) - 1;
				ydb_setenv(&ydb_cur_gbldir, &dollar_zgbldir);
				if (NULL != gv_currkey)
				{
					gv_currkey->base[0] = '\0';
					gv_currkey->prev = gv_currkey->end = 0;
				} else if (NULL != gd_header)
					gvinit();
				if (NULL != gv_target)
					gv_target->clue.end = 0;
				/* Reset any cached region-name for $zpeek since gbldir is changing and any same region name
				 * in the new gbldir should point to a different gd_region structure (and not the cached one).
				 */
				TREF(zpeek_regname_len) = 0;
			}
			break;
<<<<<<< HEAD
		case SV_ZMAXTPTIME:;
			int	num;

			/* Ensure $ZMAXTPTIME is never negative. If negative value is specified, treat it as 0 (i.e. NO timeout).
			 * This is similar to how YDBENVINDX_MAXTPTIME env var is handled in "sr_port/gtm_env_init.c".
			 */
			num = mval2i(v);
			if (0 > num)
				num = 0;
			TREF(dollar_zmaxtptime) = num;
=======
		case SV_ZMAXTPTIME:
			TREF(dollar_zmaxtptime) = mval2i(v);	/* Negative values == no timeout */
>>>>>>> eb3ea98c (GT.M V7.0-002)
			break;
		case SV_ZROUTINES:
			MV_FORCE_STR(v);
			/* The string(v) should be parsed and loaded before setting $zroutines
			 * to retain the old value in case errors occur while loading */
			zro_load(&v->str);
			break;
		case SV_ZSOURCE:
			MV_FORCE_STR(v);
			dollar_zsource.mvtype = MV_STR;
			dollar_zsource.str = v->str;
			break;
		case SV_ZTRAP:
#			ifdef GTM_TRIGGER
			if (0 < gtm_trigger_depth)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_NOZTRAPINTRIG);
#			endif
			MV_FORCE_STR(v);
			/* Save string corresponding to input mval "v" in case the string pointed to by "v->str.addr"
			 * gets shifted around by the op_newintrinsic()/gtm_newintrinsic() calls below
			 */
			lcl_mval = *v;
			if (SIZEOF(lcl_str) < lcl_mval.str.len)
			{
				tmp = (char *)malloc(lcl_mval.str.len);
				memcpy(tmp, v->str.addr, lcl_mval.str.len);
				lcl_mval.str.addr = tmp;
			} else
			{
				memcpy(lcl_str, v->str.addr, lcl_mval.str.len);
				lcl_mval.str.addr = lcl_str;
			}
			if (ztrap_new)
				op_newintrinsic(SV_ZTRAP);
			if (!v->str.len)
			{	/* Setting $ZTRAP to empty causes any current error trapping to be canceled */
				(TREF(dollar_etrap)).mvtype = (TREF(dollar_ztrap)).mvtype = MV_STR;
				(TREF(dollar_etrap)).str = (TREF(dollar_ztrap)).str = v->str;
				ztrap_explicit_null = TRUE;
				if (!dollar_zininterrupt)
					TRY_EVENT_POP;		/* not in interrupt code, so check for pending timed events */
			} else /* Ensure that $ETRAP and $ZTRAP are not both active at the same time */
			{
				ztrap_explicit_null = FALSE;
				if (!(ZTRAP_ENTRYREF & TREF(ztrap_form)))
				{
					op_commarg(v, indir_linetail);
					op_unwind();
				}
				(TREF(dollar_ztrap)).mvtype = MV_STR;
				(TREF(dollar_ztrap)).str = v->str;
				if ((TREF(dollar_etrap)).str.len > 0)
				{
					gtm_newintrinsic(&(TREF(dollar_etrap)));
					NULLIFY_TRAP(TREF(dollar_etrap));
				}
<<<<<<< HEAD
				(TREF(dollar_ztrap)).mvtype = MV_STR;
				/* Copy the saved string (pointing to C-stack or malloc storage) back to the stringpool
				 * before copying it to TREF(dollar_ztrap).str
				 */
				s2pool(&lcl_mval.str);
				(TREF(dollar_ztrap)).str = lcl_mval.str;
				if (SIZEOF(lcl_str) < lcl_mval.str.len)
					free(tmp);
=======
>>>>>>> eb3ea98c (GT.M V7.0-002)
			}
			if (ZTRAP_POP & TREF(ztrap_form))
				ztrap_save_ctxt();
			break;
		case SV_ZSTATUS:
			MV_FORCE_STR(v);
			dollar_zstatus.mvtype = MV_STR;
			dollar_zstatus.str = v->str;
			/* If we are inside an error trap handling an error as part of a "SET $ECODE=..." user action,
			 * then clear the current util_output buffer (which would hold the SETECODE error message)
			 * and replace it with the $ZSTATUS value that is being set here as it will be more useful
			 * given that the user has chosen to overwrite $ZSTATUS as part of the user defined error trap.
			 * This is done only for the first error that triggers error handling (i.e. dollar_ecode.index == 1).
			 */
			if ((1 == dollar_ecode.index) && (ERR_SETECODE == dollar_ecode.error_last_ecode))
			{
				char	*tmp, *entryref, *errortext;
				int	len;

				/* $ZSTATUS is usually in the form ERRNUM,ENTRYREF,ERRORTEXT.
				 * This will mirror the format of the error displayed when not in direct mode.
				 */
				len = dollar_zstatus.str.len;
				tmp = (char *)malloc(len + 1);
				memcpy(tmp, dollar_zstatus.str.addr, len);
				tmp[len] = '\0';
				entryref = strchr(tmp, ',');
				errortext = (NULL != entryref) ? strchr(entryref + 1, ',') : NULL;
				util_out_print(NULL, RESET);	/* Clear whatever message was in buffer previously */
				if (NULL != errortext)
				{	/* $ZSTATUS is in the form ERRNUM,ENTRYREF,ERRORTEXT.
					 * Copy over ERRORTEXT \n \t\t ENTRYREF to the util_output buffer.
					 * This will mirror the format of the error displayed when not in direct mode.
					 */
					char	*tmp2;

					len = errortext - entryref;
					tmp2 = (char *)malloc(len);
					len--;
					memcpy(tmp2, entryref + 1, len);
					tmp2[len] = '\0';
					util_out_print(errortext + 1, NOFLUSH_OUT);
					/* The below simulates a RTSLOC error */
					util_out_print("!/!_!_At M source location ", NOFLUSH_OUT);	/* !/ is \n, !_ is \t */
					util_out_print(tmp2, NOFLUSH_OUT);
					free(tmp2);
				} else {
					/* $ZSTATUS is not in the form ERRNUM,ENTRYREF,ERRORTEXT.
					 * Likely because the user defined error trap is not abiding by the convention
					 * when setting $ZSTATUS to a value. Copy over the entire $ZSTATUS text into
					 * the util_output buffer in this case.
					 */
					util_out_print(tmp, NOFLUSH_OUT);
				}
				free(tmp);
			}
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
#			ifdef UTF8_SUPPORTED
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
					RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_INVECODEVAL, 2, v->str.len, v->str.addr);
				}
			}
			if (v->str.len > 0)
			{
				ecode_add(&v->str);
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(2) ERR_SETECODE, 0);
			} else
			{
				NULLIFY_DOLLAR_ECODE;	/* reset $ECODE related variables to correspond to $ECODE = NULL state */
				NULLIFY_ERROR_FRAME;	/* we are no more in error-handling mode */
				/* A tp timeout was deferred. Now that we are clear of error handling and no job interrupt
				 * is in process, allow the timeout to be recognized.
				 */
				if (!dollar_zininterrupt)
					TRY_EVENT_POP;		/* not in interrupt code, so check for pending timed events */
			}
			break;
		case SV_ETRAP:
			MV_FORCE_STR(v);
			ztrap_explicit_null = FALSE;
			if ((TREF(dollar_ztrap)).str.len > 0)
			{	/* replacing ZTRAP with ETRAP */
				NULLIFY_TRAP(TREF(dollar_ztrap));
				(TREF(dollar_etrap))= default_etrap;	/* want change, so use default value in case of bad value */
			}
			if (v->str.len)
			{	/* check we have valid code */
				op_commarg(v, indir_linetail);
				op_unwind();
			}	/* set $etrap="" clears any current value, but doesn't cancal all trapping the way $ZTRAP="" does */
			(TREF(dollar_etrap)).mvtype = MV_STR;
			(TREF(dollar_etrap)).str = v->str;
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
			MV_FORCE_STR(v);
			if (0 == v->str.len)
				dollar_system = dollar_system_initial;	/* input is empty: set back to initial value */
			else if ((MAX_TRANS_NAME_LEN + STR_LIT_LEN("47,")) > (i = (dollar_system_initial.str.len + v->str.len)))
			{	/* value fits, so append the value; WARNING assignment above */
				ENSURE_STP_FREE_SPACE(i);
				memcpy(stringpool.free, dollar_system_initial.str.addr, dollar_system_initial.str.len);
				dollar_system.str.addr = (char *)stringpool.free;
				stringpool.free += dollar_system_initial.str.len;
				memcpy((stringpool.free), v->str.addr, v->str.len);
				dollar_system.str.len = i;
				stringpool.free += v->str.len;
			} else
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_SYSTEMVALUE, 2, v->str.len, v->str.addr);
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
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_SETINTRIGONLY, 2, RTS_ERROR_TEXT("$ZTVALUE"));
			if (dollar_ztriggerop != &gvtr_cmd_mval[GVTR_CMDTYPE_SET])
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETINSETTRIGONLY, 2, RTS_ERROR_TEXT("$ZTVALUE"));
			assert(0 < gtm_trigger_depth);
			memcpy(dollar_ztvalue, v, SIZEOF(mval));
			dollar_ztvalue->mvtype &= ~MV_ALIASCONT;	/* Make sure to shut off alias container flag on copy */
			assert(NULL != ztvalue_changed_ptr);
			*ztvalue_changed_ptr = TRUE;
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTWORMHOLE:
#			ifdef GTM_TRIGGER
			MV_FORCE_STR(v);
			/* See jnl.h for why MAX_ZTWORMHOLE_SIZE should be less than minimum alignsize */
			assert(MAX_ZTWORMHOLE_SIZE < (JNL_MIN_ALIGNSIZE * DISK_BLOCK_SIZE));
			if (MAX_ZTWORMHOLE_SIZE < v->str.len)
				RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(4) ERR_ZTWORMHOLE2BIG, 2, v->str.len, MAX_ZTWORMHOLE_SIZE);
			dollar_ztwormhole.mvtype = MV_STR;
			dollar_ztwormhole.str = v->str;
			write_ztworm_jnl_rec = TRUE;
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZTSLATE:
#			ifdef GTM_TRIGGER
			assert(!dollar_tlevel || (tstart_trigger_depth <= gtm_trigger_depth));
			if (!dollar_tlevel || (tstart_trigger_depth == gtm_trigger_depth))
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_SETINTRIGONLY, 2, RTS_ERROR_TEXT("$ZTSLATE"));
			assert(0 < gtm_trigger_depth);
			MV_FORCE_DEFINED(v);
			memcpy((char *)&dollar_ztslate, v, SIZEOF(mval));
			dollar_ztslate.mvtype &= ~MV_ALIASCONT;	/* Make sure to shut off alias container flag on copy */
			break;
#			else
			RTS_ERROR_CSA_ABT(NULL, VARLSTCNT(1) ERR_UNIMPLOP);
#			endif
		case SV_ZSTRPLLIM:
			previous_gtm_strpllim = stringpool.strpllim;
			stringpool.strpllim = MV_FORCE_INT(v);
			if (STP_GCOL_TRIGGER_FLOOR > stringpool.strpllim)
				stringpool.strpllim = STP_GCOL_TRIGGER_FLOOR;
			if ((stringpool.strpllim <= 0) || (stringpool.strpllim >= previous_gtm_strpllim))
				stringpool.strpllimwarned =  FALSE;
			break;
		case SV_ZMALLOCLIM:
			MV_FORCE_INT(v);
			tmp = mval2i(v);
			rtmp = (size_t)getstorage();
			if (0 > tmp)					/* negative gives half the OS limit */
				tmp = rtmp / (size_t)2;			/* see gtm_malloc_src.h MALLOC macro comment on halving */
			else if (rtmp < (size_t)tmp)
				tmp = (int)rtmp;
			else if ((0 != tmp) && (tmp < MIN_MALLOC_LIM))
				tmp = MIN_MALLOC_LIM;
			malloccrit_issued = ((0 == tmp) || ((size_t)tmp) >= zmalloclim) ? 0 : malloccrit_issued;
			zmalloclim = (size_t)tmp;			/* reset malloccrit_issued above if SET raised/kept limit */
			break;
		case SV_ZTIMEOUT:
			check_and_set_ztimeout(v);
			break;
		case SV_ZCMDLINE:
			MV_FORCE_STR(v);
			dollar_zcmdline.mvtype = MV_STR;
			dollar_zcmdline.str = v->str;
			break;
		default:
			assertpro(FALSE && varnum);
	}
	return;
}
