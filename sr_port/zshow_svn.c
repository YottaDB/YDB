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

#include "gtm_unistd.h"
#include "gtm_string.h"
#include "gtm_limits.h"

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
#include "jnl.h"
#include <rtnhdr.h>
#include "stack_frame.h"
#include "svnames.h"
#include "mlkdef.h"
#include "zshow.h"
#include "buddy_list.h"		/* needed for tp.h */
#include "iottdef.h"		/* needed for tp.h */
#include "hashtab_int4.h"	/* needed for tp.h */
#include "tp.h"
#include "op.h"
#include "mvalconv.h"
#include "zroutines.h"
#include "getstorage.h"
#include "get_command_line.h"
#include "getzposition.h"
#include "dollar_zlevel.h"
#include "get_ret_targ.h"
#include "error_trap.h"
#include "setzdir.h"
#include "get_reference.h"
#include "sgtm_putmsg.h"
#include "dollar_quit.h"

#define ESC_OFFSET 		4
#define MAX_COMMAND_LINE_LENGTH 255
#define ZS_ONE_OUT(V,TEXT) 	((V)->len = 1, (V)->addr = (TEXT), zshow_output(output,V))
#define ZS_VAR_EQU(V,TEXT) 	((V)->len = SIZEOF(TEXT) - 1, (V)->addr = TEXT, \
				 zshow_output(output,(V)), ZS_ONE_OUT((V),equal_text))

static readonly char equal_text[] = {'='};
static readonly char device_text[] = "$DEVICE";
static readonly char ecode_text[] = "$ECODE";
static readonly char estack_text[] = "$ESTACK";
static readonly char etrap_text[] = "$ETRAP";
static readonly char horolog_text[] = "$HOROLOG";
static readonly char io_text[] = "$IO";
static readonly char job_text[] = "$JOB";
static readonly char key_text[] = "$KEY";
static readonly char principal_text[] = "$PRINCIPAL";
static readonly char quit_text[] = "$QUIT";
static readonly char reference_text[] = "$REFERENCE";
static readonly char stack_text[] = "$STACK";
static readonly char storage_text[] = "$STORAGE";
static readonly char system_text[] = "$SYSTEM";
static readonly char test_text[] = "$TEST";
static readonly char tlevel_text[] = "$TLEVEL";
static readonly char trestart_text[] = "$TRESTART";
static readonly char x_text[] = "$X";
static readonly char y_text[] = "$Y";
static readonly char za_text[] = "$ZA";
static readonly char zallocstor_text[] = "$ZALLOCSTOR";
static readonly char zb_text[] = "$ZB";
static readonly char zchset_text[] = "$ZCHSET";
static readonly char zcmdline_text[] = "$ZCMDLINE";
static readonly char zcompile_text[] = "$ZCOMPILE";
static readonly char zcstatus_text[] = "$ZCSTATUS";
static readonly char zdate_form_text[] = "$ZDATEFORM";
static readonly char zdirectory_text[] = "$ZDIRECTORY";
static readonly char zeditor_text[] = "$ZEDITOR";
static readonly char zeof_text[] = "$ZEOF";
static readonly char zerror_text[] = "$ZERROR";
static readonly char zgbldir_text[] = "$ZGBLDIR";
static readonly char zininterrupt_text[] = "$ZININTERRUPT";
static readonly char zinterrupt_text[] = "$ZINTERRUPT";
static readonly char zio_text[] = "$ZIO";
static readonly char zjob_text[] = "$ZJOB";
static readonly char zlevel_text[] = "$ZLEVEL";
static readonly char zmaxtptime_text[] = "$ZMAXTPTIME";
static readonly char zmode_text[] = "$ZMODE";
static readonly char zpatnumeric_text[] = "$ZPATNUMERIC";
static readonly char zproc_text[] = "$ZPROCESS";
static readonly char zprompt_text[] = "$ZPROMPT";
static readonly char zpos_text[] = "$ZPOSITION";
static readonly char zquit_text[] = "$ZQUIT";
static readonly char zrealstor_text[] = "$ZREALSTOR";
static readonly char zroutines_text[] = "$ZROUTINES";
static readonly char zsource_text[] = "$ZSOURCE";
static readonly char zstatus_text[] = "$ZSTATUS";
static readonly char zstep_text[] = "$ZSTEP";
static readonly char zsystem_text[] = "$ZSYSTEM";
#ifdef GTM_TRIGGER
static readonly char ztname_text[] = "$ZTNAME";
static readonly char ztdata_text[] = "$ZTDATA";
#endif
static readonly char ztexit_text[] = "$ZTEXIT";
GTMTRIG_ONLY(static readonly char ztlevel_text[] = "$ZTLEVEL";)
#ifdef GTM_TRIGGER
static readonly char ztoldval_text[] = "$ZTOLDVAL";
#endif
static readonly char ztrap_text[] = "$ZTRAP";
#ifdef GTM_TRIGGER
static readonly char ztriggerop_text[] = "$ZTRIGGEROP";
static readonly char ztslate_text[] = "$ZTSLATE";
static readonly char ztupdate_text[] = "$ZTUPDATE";
static readonly char ztvalue_text[] = "$ZTVALUE";
static readonly char ztwormhole_text[] = "$ZTWORMHOLE";
#endif
static readonly char zusedstor_text[] = "$ZUSEDSTOR";
static readonly char zversion_text[] = "$ZVERSION";
static readonly char zyerror_text[] = "$ZYERROR";
static readonly char zonlnrlbk_text[] = "$ZONLNRLBK";
static readonly char arrow_text[] = "->";

GBLREF mval		dollar_zdir;
GBLREF mval		dollar_zproc;
GBLREF stack_frame	*frame_pointer;
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
GBLREF mval		dollar_etrap, dollar_estack_delta, dollar_zerror, dollar_zyerror, dollar_system;
GBLREF mval		dollar_zinterrupt, dollar_ztexit;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF int4		zdir_form;
GBLREF size_t		totalAlloc;
GBLREF size_t		totalRmalloc;
GBLREF size_t		totalUsed;
GBLREF mstr		dollar_zchset;
GBLREF mstr		dollar_zpatnumeric;
GBLREF boolean_t	dollar_zquit_anyway;
#ifdef GTM_TRIGGER
GBLREF mstr		*dollar_ztname;
GBLREF mval		*dollar_ztdata;
GBLREF mval		*dollar_ztoldval;
GBLREF mval		*dollar_ztriggerop;
GBLREF mval		dollar_ztslate;
GBLREF mval		*dollar_ztupdate;
GBLREF mval		*dollar_ztvalue;
GBLREF mval		dollar_ztwormhole;
GBLREF int4		gtm_trigger_depth;
#endif

LITREF mval		literal_zero, literal_one, literal_null;
LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

error_def(ERR_ZDIROUTOFSYNC);

void zshow_svn(zshow_out *output, int one_sv)
{
	mstr		x;
	mval		var, zdir;
	io_log_name	*tl;
       	stack_frame	*fp;
	int 		count, save_dollar_zlevel;
	char		*c1, *c2;
	char		zdir_error[3 * GTM_MAX_DIR_LEN + 128]; /* PATH_MAX + "->" + GTM-W-ZDIROUTOFSYNC, <text of ZDIROUTOFSYNC> */
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	switch(one_sv)
	{
		case SV_ALL:
		case SV_DEVICE:
			get_dlr_device(&var);
			ZS_VAR_EQU(&x, device_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ECODE:
			ecode_get(-1, &var);
			ZS_VAR_EQU(&x, ecode_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ESTACK:
			save_dollar_zlevel = dollar_zlevel();
			count = (save_dollar_zlevel - 1) - dollar_estack_delta.m[0];
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, estack_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ETRAP:
			var.mvtype = MV_STR;
			var.str = dollar_etrap.str;
			ZS_VAR_EQU(&x, etrap_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_HOROLOG:
			op_horolog(&var);
			ZS_VAR_EQU(&x, horolog_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_IO:
			var.str.addr = io_curr_device.in->name->dollar_io;
			var.str.len = io_curr_device.in->name->len;
			/*** The following should be in the I/O code ***/
			if (ESC == *var.str.addr)
			{
				if (5 > var.str.len)
					var.str.len = 0;
				else
				{
					var.str.addr += ESC_OFFSET;
					var.str.len -= ESC_OFFSET;
				}
			}
			var.mvtype = MV_STR;
			ZS_VAR_EQU(&x, io_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_JOB:
			ZS_VAR_EQU(&x, job_text);
			mval_write(output, &dollar_job, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_KEY:
			get_dlr_key(&var);
			ZS_VAR_EQU(&x, key_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_PRINCIPAL:
			if (dollar_principal)
				tl = dollar_principal;
			else
				tl = io_root_log_name->iod->trans_name;
			var.str.addr = tl->dollar_io;
			var.str.len = tl->len;
			/*** The following should be in the I/O code ***/
			if (ESC == *var.str.addr)
			{
				if (5 > var.str.len)
					var.str.len = 0;
				else
				{
					var.str.addr += ESC_OFFSET;
					var.str.len -= ESC_OFFSET;
				}
			}
			var.mvtype = MV_STR;
			ZS_VAR_EQU(&x, principal_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_QUIT:
			count = dollar_quit();
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, quit_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_REFERENCE:
			get_reference(&var);
			ZS_VAR_EQU(&x, reference_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_STACK:
			save_dollar_zlevel = dollar_zlevel();
			count = (save_dollar_zlevel - 1);
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, stack_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_STORAGE:
			/* double2mval(&var, getstorage()); Causes issues with unaligned stack on x86_64 - remove until fixed */
			i2mval(&var, getstorage());
			ZS_VAR_EQU(&x, storage_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_SYSTEM:
			var.mvtype = MV_STR;
			var.str = dollar_system.str;
			ZS_VAR_EQU(&x, system_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_TEST:
			i2mval(&var, (int)op_dt_get());
			ZS_VAR_EQU(&x, test_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_TLEVEL:
			count = (int)dollar_tlevel;
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, tlevel_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_TRESTART:
			MV_FORCE_MVAL(&var, (int)((MAX_VISIBLE_TRESTART < dollar_trestart)
				? MAX_VISIBLE_TRESTART : dollar_trestart));
			ZS_VAR_EQU(&x, trestart_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_X:
			count = (int)io_curr_device.out->dollar.x;
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, x_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_Y:
			count = (int)io_curr_device.out->dollar.y;
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, y_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZA:
			count = (int)io_curr_device.in->dollar.za;
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, za_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZALLOCSTOR:
			count = (int)totalAlloc;	/* WARNING: downcasting possible 64bit value to 32bits */
			MV_FORCE_UMVAL(&var, (unsigned int)count);
			ZS_VAR_EQU(&x, zallocstor_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZB:
			c1 = (char *)io_curr_device.in->dollar.zb;
			c2 = c1 + SIZEOF(io_curr_device.in->dollar.zb);
			var.mvtype = MV_STR;
			var.str.addr = (char *)io_curr_device.in->dollar.zb;
			while (c1 < c2 && *c1)
				c1++;
			var.str.len = INTCAST((char *)c1 - var.str.addr);
			ZS_VAR_EQU(&x, zb_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZCHSET:
			var.mvtype = MV_STR;
			var.str = dollar_zchset;
			ZS_VAR_EQU(&x, zchset_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZCMDLINE:
			get_command_line(&var, TRUE);	/* TRUE indicates $ZCMDLINE (i.e. processed not actual command line) */
			ZS_VAR_EQU(&x, zcmdline_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZCOMPILE:
			var.mvtype = MV_STR;
			var.str = TREF(dollar_zcompile);
			ZS_VAR_EQU(&x, zcompile_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZCSTATUS:
			MV_FORCE_MVAL(&var, TREF(dollar_zcstatus));
			ZS_VAR_EQU(&x, zcstatus_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZDATE_FORM:
			MV_FORCE_MVAL(&var, TREF(zdate_form));
			ZS_VAR_EQU(&x, zdate_form_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZDIR:
			ZS_VAR_EQU(&x, zdirectory_text);
			setzdir(NULL, &zdir);
			if (zdir.str.len != dollar_zdir.str.len || 0 != memcmp(zdir.str.addr, dollar_zdir.str.addr, zdir.str.len))
			{
				memcpy(zdir_error, zdir.str.addr, zdir.str.len);
				memcpy(&zdir_error[zdir.str.len], arrow_text, STR_LIT_LEN(arrow_text));
				sgtm_putmsg(&zdir_error[zdir.str.len + STR_LIT_LEN(arrow_text)], VARLSTCNT(6) ERR_ZDIROUTOFSYNC, 4,
					zdir.str.len, zdir.str.addr, dollar_zdir.str.len, dollar_zdir.str.addr);
				zdir.str.addr = zdir_error;
				zdir.str.len = STRLEN(zdir_error) - 1; /* eliminate trailing '\n' */
			}
			SKIP_DEVICE_IF_NOT_NEEDED(&zdir);
			mval_write(output, &zdir, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZEDITOR:
			MV_FORCE_MVAL(&var, dollar_zeditor);
			ZS_VAR_EQU(&x, zeditor_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZEOF:
			ZS_VAR_EQU(&x, zeof_text);
			mval_write(output, io_curr_device.in->dollar.zeof ? (mval *)&literal_one : (mval *)&literal_zero, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZERROR:
			var.mvtype = MV_STR;
			var.str = dollar_zerror.str;
			ZS_VAR_EQU(&x, zerror_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZGBLDIR:
			ZS_VAR_EQU(&x, zgbldir_text);
			mval_write(output, &dollar_zgbldir, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZININTERRUPT:
			MV_FORCE_MVAL(&var, dollar_zininterrupt);
			ZS_VAR_EQU(&x, zininterrupt_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZINTERRUPT:
			var.mvtype = MV_STR;
			var.str = dollar_zinterrupt.str;
			ZS_VAR_EQU(&x, zinterrupt_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZIO:
			var.mvtype = MV_STR;
			/* NOTE:	This is **NOT** equivalent to :
			 *		io_curr_log_name->dollar_io
			 */
			var.str.addr = io_curr_device.in->trans_name->dollar_io;
			var.str.len = io_curr_device.in->trans_name->len;
			if (*var.str.addr == ESC)
			{
				if (5 > var.str.len)
					var.str.len = 0;
				else
				{
					var.str.addr += ESC_OFFSET;
					var.str.len -= ESC_OFFSET;
				}
			}
			ZS_VAR_EQU(&x, zio_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZJOB:
			MV_FORCE_UMVAL(&var, dollar_zjob);
			ZS_VAR_EQU(&x, zjob_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZLEVEL:
			save_dollar_zlevel = dollar_zlevel();
			MV_FORCE_MVAL(&var, save_dollar_zlevel);
			ZS_VAR_EQU(&x, zlevel_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZMAXTPTIME:
			MV_FORCE_MVAL(&var, TREF(dollar_zmaxtptime));
			ZS_VAR_EQU(&x, zmaxtptime_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZMODE:
			ZS_VAR_EQU(&x, zmode_text);
			mval_write(output, TADR(dollar_zmode), TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
#		ifdef UNIX
		case SV_ZONLNRLBK:
			count = (int)(TREF(dollar_zonlnrlbk));
			MV_FORCE_MVAL(&var, count);
			ZS_VAR_EQU(&x, zonlnrlbk_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
#		endif
		/* CAUTION: fall through */
		case SV_ZPATNUMERIC:
			var.mvtype = MV_STR;
			var.str = dollar_zpatnumeric;
			ZS_VAR_EQU(&x, zpatnumeric_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZPOS:
			getzposition(&var);
			ZS_VAR_EQU(&x, zpos_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZPROC:
			ZS_VAR_EQU(&x, zproc_text);
			mval_write(output, &dollar_zproc, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_PROMPT:
			var.mvtype = MV_STR;
			var.str.addr = (TREF(gtmprompt)).addr;
			var.str.len = (TREF(gtmprompt)).len;
			ZS_VAR_EQU(&x, zprompt_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZQUIT:
			MV_FORCE_MVAL(&var, dollar_zquit_anyway);
			ZS_VAR_EQU(&x, zquit_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZREALSTOR:
			count = (int)totalRmalloc;	/* WARNING: downcasting possible 64bit value to 32bits */
			MV_FORCE_UMVAL(&var, (unsigned int)count);
			ZS_VAR_EQU(&x, zrealstor_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZROUTINES:
			if (!TREF(zro_root))
				zro_init();
			var.mvtype = MV_STR;
			var.str = TREF(dollar_zroutines);
			ZS_VAR_EQU(&x, zroutines_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZSOURCE:
			ZS_VAR_EQU(&x, zsource_text);
			mval_write(output, &dollar_zsource, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZSTATUS:
			ZS_VAR_EQU(&x, zstatus_text);
			mval_write(output, &dollar_zstatus, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZSTEP:
			ZS_VAR_EQU(&x, zstep_text);
			mval_write(output, &dollar_zstep, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZSYSTEM:
			MV_FORCE_MVAL(&var, dollar_zsystem);
			ZS_VAR_EQU(&x, zsystem_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
#		ifdef GTM_TRIGGER
		case SV_ZTCODE:		/* deprecated */
		/* CAUTION: fall through */
		case SV_ZTNAME:
			if (NULL != dollar_ztname)
			{
				var.mvtype = MV_STR;
				var.str.addr = dollar_ztname->addr;
				var.str.len = dollar_ztname->len;
			} else
				memcpy(&var, &literal_null, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztname_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTDATA:
			if (NULL != dollar_ztdata)
			{
				var.mvtype = MV_STR;
				var.str = dollar_ztdata->str;
			} else
				memcpy(&var, &literal_zero, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztdata_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
#		endif
		/* CAUTION: fall through */
		case SV_ZTEXIT:
			var.mvtype = MV_STR;
			var.str = dollar_ztexit.str;
			ZS_VAR_EQU(&x, ztexit_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
#		ifdef GTM_TRIGGER
		case SV_ZTLEVEL:
			MV_FORCE_MVAL(&var, gtm_trigger_depth);
			ZS_VAR_EQU(&x, ztlevel_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTOLDVAL:
			if (NULL != dollar_ztoldval)
			{
				var.mvtype = MV_STR;
				var.str = dollar_ztoldval->str;
			} else
				memcpy(&var, &literal_null, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztoldval_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
#		endif
		/* CAUTION: fall through */
		case SV_ZTRAP:
			var.mvtype = MV_STR;
			var.str = dollar_ztrap.str;
			ZS_VAR_EQU(&x, ztrap_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
#		ifdef GTM_TRIGGER
		case SV_ZTRIGGEROP:
			if (NULL != dollar_ztriggerop)
			{
				var.mvtype = MV_STR;
				var.str = dollar_ztriggerop->str;
			} else
				memcpy(&var, &literal_null, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztriggerop_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTSLATE:
			var.mvtype = MV_STR;
			var.str = dollar_ztslate.str;
			ZS_VAR_EQU(&x, ztslate_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTUPDATE:
			if (NULL != dollar_ztupdate)
			{
				var.mvtype = MV_STR;
				var.str = dollar_ztupdate->str;
			} else
				memcpy(&var, &literal_null, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztupdate_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTVALUE:
			if (NULL != dollar_ztvalue)
			{
				var.mvtype = MV_STR;
				var.str = dollar_ztvalue->str;
			} else
				memcpy(&var, &literal_null, SIZEOF(mval));
			ZS_VAR_EQU(&x, ztvalue_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZTWORMHOLE:
			var.mvtype = MV_STR;
			var.str = dollar_ztwormhole.str;
			ZS_VAR_EQU(&x, ztwormhole_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
#		endif
		/* CAUTION: fall through */
		case SV_ZUSEDSTOR:
			count = (int)totalUsed;		/* WARNING: downcasting possible 64bit value to 32bits */
			MV_FORCE_UMVAL(&var, (unsigned int)count);
			ZS_VAR_EQU(&x, zusedstor_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		/* CAUTION: fall through */
		case SV_ZVERSION:
			var.mvtype = MV_STR;
			var.str.addr = (char *)gtm_release_name;
			var.str.len = gtm_release_name_len;
			ZS_VAR_EQU(&x, zversion_text);
			mval_write(output, &var, TRUE);
			if (SV_ALL != one_sv)
				break;
		case SV_ZYERROR:
			var.mvtype = MV_STR;
			var.str = dollar_zyerror.str;
			ZS_VAR_EQU(&x, zyerror_text);
			mval_write(output, &var, TRUE);
			break;
		default:
			GTMASSERT;

	}
}
