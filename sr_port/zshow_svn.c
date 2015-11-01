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
#include "rtnhdr.h"
#include "stack_frame.h"
#include "svnames.h"
#include "mlkdef.h"
#include "zshow.h"
#include "hashtab.h"		/* needed for tp.h */
#include "buddy_list.h"		/* needed for tp.h */
#include "iottdef.h"		/* needed for tp.h */
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

#define ESC_OFFSET 		4
#define MAX_COMMAND_LINE_LENGTH 255
#define ZS_ONE_OUT(V,TEXT) 	((V)->len = 1, (V)->addr = (TEXT), zshow_output(output,V))
#define ZS_VAR_EQU(V,TEXT) 	((V)->len = sizeof(TEXT) - 1, (V)->addr = TEXT, \
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
static readonly char zb_text[] = "$ZB";
static readonly char zcmdline_text[] = "$ZCMDLINE";
static readonly char zcompile_text[] = "$ZCOMPILE";
static readonly char zcstatus_text[] = "$ZCSTATUS";
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
static readonly char zmode_text[] = "$ZMODE";
static readonly char zproc_text[] = "$ZPROCESS";
static readonly char zprompt_text[] = "$ZPROMPT";
static readonly char zpos_text[] = "$ZPOSITION";
static readonly char zroutines_text[] = "$ZROUTINES";
static readonly char zsource_text[] = "$ZSOURCE";
static readonly char zstatus_text[] = "$ZSTATUS";
static readonly char zsystem_text[] = "$ZSYSTEM";
static readonly char ztrap_text[] = "$ZTRAP";
static readonly char zversion_text[] = "$ZVERSION";
static readonly char zyerror_text[] = "$ZYERROR";
static readonly char arrow_text[] = "->";

GBLREF mval		dollar_zdir;
GBLREF mval		dollar_zmode;
GBLREF mval		dollar_zproc;
GBLREF mstr		dollar_zroutines;
GBLREF mstr		dollar_zcompile;
GBLREF stack_frame	*frame_pointer;
GBLREF io_pair		io_curr_device;
GBLREF io_log_name	*io_root_log_name;
GBLREF io_log_name	*dollar_principal;
GBLREF mval		dollar_ztrap;
GBLREF mval		dollar_zgbldir;
GBLREF mval		dollar_job;
GBLREF uint4		dollar_zjob;
GBLREF mval		dollar_zstatus;
GBLREF char		*zro_root;  /* ACTUALLY some other pointer type! */
GBLREF mstr		gtmprompt;
GBLREF mstr		dollar_zsource;
GBLREF int4		dollar_zsystem;
GBLREF int4		dollar_zcstatus;
GBLREF int4		dollar_zeditor;
GBLREF short		dollar_tlevel;
GBLREF short		dollar_trestart;
GBLREF mval		dollar_etrap;
GBLREF mval		dollar_zerror;
GBLREF mval		dollar_zyerror;
GBLREF mval		dollar_system;
GBLREF mval		dollar_zinterrupt;
GBLREF boolean_t	dollar_zininterrupt;
GBLREF int4		zdir_form;

LITREF mval		literal_zero,literal_one;
LITREF char		gtm_release_name[];
LITREF int4		gtm_release_name_len;

void zshow_svn(zshow_out *output)
{
	mstr		x;
	mval		var, zdir;
	io_log_name	*tl;
       	stack_frame	*fp;
	int 		count, save_dollar_zlevel;
	char		*c1, *c2;
	char		zdir_error[3 * GTM_MAX_DIR_LEN + 128]; /* PATH_MAX + "->" + GTM-W-ZDIROUTOFSYNC, <text of ZDIROUTOFSYNC> */

	error_def(ERR_ZDIROUTOFSYNC);

	/* SV_DEVICE */
		get_dlr_device(&var);
		ZS_VAR_EQU(&x, device_text);
		mval_write(output, &var, TRUE);
	/* SV_ECODE */
		dollar_ecode_build(-1, &var);
		ZS_VAR_EQU(&x, ecode_text);
		mval_write(output, &var, TRUE);
	/* SV_ESTACK should go here */
	/* SV_ETRAP */
		var.mvtype = MV_STR;
		var.str = dollar_etrap.str;
		ZS_VAR_EQU(&x, etrap_text);
		mval_write(output, &var, TRUE);
	/* SV_HOROLOG */
		op_horolog(&var);
		ZS_VAR_EQU(&x, horolog_text);
		mval_write(output, &var, TRUE);
	/* SV_IO */
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
	/* SV_JOB */
		ZS_VAR_EQU(&x, job_text);
		mval_write(output, &dollar_job, TRUE);
	/* SV_KEY */
		get_dlr_key(&var);
		ZS_VAR_EQU(&x, key_text);
		mval_write(output, &var, TRUE);
	/* SV_PRINCIPAL */
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
	/* SV_QUIT */
		count = ((NULL == get_ret_targ()) ? 0 : 1);
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, quit_text);
		mval_write(output, &var, TRUE);
	/* SV_REFERENCE */
		get_reference(&var);
		ZS_VAR_EQU(&x, reference_text);
		mval_write(output, &var, TRUE);
	/* SV_STACK */
		count = (save_dollar_zlevel = dollar_zlevel()) - 1;
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, stack_text);
		mval_write(output, &var, TRUE);
	/* SV_STORAGE */
		i2mval(&var, getstorage());
		ZS_VAR_EQU(&x, storage_text);
		mval_write(output, &var, TRUE);
	/* SV_SYSTEM */
		var.mvtype = MV_STR;
		var.str = dollar_system.str;
		ZS_VAR_EQU(&x, system_text);
		mval_write(output, &var, TRUE);
	/* SV_TEST */
		i2mval(&var, (int)op_dt_get());
		ZS_VAR_EQU(&x, test_text);
		mval_write(output, &var, TRUE);
	/* SV_TLEVEL */
		count = (int)dollar_tlevel;
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, tlevel_text);
		mval_write(output, &var, TRUE);
	/* SV_TRESTART */
		MV_FORCE_MVAL(&var, (int)((MAX_VISIBLE_TRESTART < dollar_trestart) ? MAX_VISIBLE_TRESTART : dollar_trestart));
		ZS_VAR_EQU(&x, trestart_text);
		mval_write(output, &var, TRUE);
	/* SV_X */
		count = (int)io_curr_device.out->dollar.x;
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, x_text);
		mval_write(output, &var, TRUE);
	/* SV_Y */
		count = (int)io_curr_device.out->dollar.y;
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, y_text);
		mval_write(output, &var, TRUE);
	/* SV_ZA */
		count = (int)io_curr_device.in->dollar.za;
		MV_FORCE_MVAL(&var, count);
		ZS_VAR_EQU(&x, za_text);
		mval_write(output, &var, TRUE);
	/* SV_ZB */
		c1 = (char *)io_curr_device.in->dollar.zb;
		c2 = c1 + sizeof(io_curr_device.in->dollar.zb);
		var.mvtype = MV_STR;
		var.str.addr = (char *)io_curr_device.in->dollar.zb;
		while (c1 < c2 && *c1)
			c1++;
		var.str.len = (char *)c1 - var.str.addr;
		ZS_VAR_EQU(&x, zb_text);
		mval_write(output, &var, TRUE);
	/* SV_ZCMDLINE */
		get_command_line(&var);
		ZS_VAR_EQU(&x, zcmdline_text);
		mval_write(output, &var, TRUE);
	/* SV_ZCOMPILE */
		var.mvtype = MV_STR;
		var.str = dollar_zcompile;
		ZS_VAR_EQU(&x, zcompile_text);
		mval_write(output, &var, TRUE);
	/* SV_ZCSTATUS */
		MV_FORCE_MVAL(&var, dollar_zcstatus);
		ZS_VAR_EQU(&x, zcstatus_text);
		mval_write(output, &var, TRUE);
	/* SV_ZDIR */
		ZS_VAR_EQU(&x, zdirectory_text);
		setzdir(NULL, &zdir);
		if (zdir.str.len != dollar_zdir.str.len || 0 != memcmp(zdir.str.addr, dollar_zdir.str.addr, zdir.str.len))
		{
			memcpy(zdir_error, zdir.str.addr, zdir.str.len);
			memcpy(&zdir_error[zdir.str.len], arrow_text, STR_LIT_LEN(arrow_text));
			sgtm_putmsg(&zdir_error[zdir.str.len + STR_LIT_LEN(arrow_text)], VARLSTCNT(6) ERR_ZDIROUTOFSYNC, 4,
					zdir.str.len, zdir.str.addr, dollar_zdir.str.len, dollar_zdir.str.addr);
			zdir.str.addr = zdir_error;
			zdir.str.len = strlen(zdir_error) - 1; /* eliminate trailing '\n' */
		}
		SKIP_DEVICE_IF_NOT_NEEDED(&zdir);
		mval_write(output, &zdir, TRUE);
	/* SV_ZEDITOR */
		MV_FORCE_MVAL(&var, dollar_zeditor);
		ZS_VAR_EQU(&x, zeditor_text);
		mval_write(output, &var, TRUE);
	/* SV_ZEOF */
		ZS_VAR_EQU(&x, zeof_text);
		mval_write(output, io_curr_device.in->dollar.zeof ? &literal_one : &literal_zero, TRUE);
	/* SV_ZERROR */
		var.mvtype = MV_STR;
		var.str = dollar_zerror.str;
		ZS_VAR_EQU(&x, zerror_text);
		mval_write(output, &var, TRUE);
	/* SV_ZGBLDIR */
		ZS_VAR_EQU(&x, zgbldir_text);
		mval_write(output, &dollar_zgbldir, TRUE);
	/* SV_ZININTERRUPT */
		MV_FORCE_MVAL(&var, dollar_zininterrupt);
		ZS_VAR_EQU(&x, zininterrupt_text);
		mval_write(output, &var, TRUE);
	/* SV_ZINTERRUPT */
		var.mvtype = MV_STR;
		var.str = dollar_zinterrupt.str;
		ZS_VAR_EQU(&x, zinterrupt_text);
		mval_write(output, &var, TRUE);
	/* SV_ZIO */
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
	/* SV_ZJOB */
		MV_FORCE_ULONG_MVAL(&var, dollar_zjob);
		ZS_VAR_EQU(&x, zjob_text);
		mval_write(output, &var, TRUE);
	/* SV_ZLEVEL */
		MV_FORCE_MVAL(&var, save_dollar_zlevel);
		ZS_VAR_EQU(&x, zlevel_text);
		mval_write(output, &var, TRUE);
	/* SV_ZMODE */
		ZS_VAR_EQU(&x, zmode_text);
		mval_write(output, &dollar_zmode, TRUE);
	/* SV_ZPOS */
		getzposition(&var);
		ZS_VAR_EQU(&x, zpos_text);
		mval_write(output, &var, TRUE);
	/* SV_ZPROC */
		ZS_VAR_EQU(&x, zproc_text);
		mval_write(output, &dollar_zproc, TRUE);
	/* SV_PROMPT */
		var.mvtype = MV_STR;
		var.str.addr = gtmprompt.addr;
		var.str.len = gtmprompt.len;
		ZS_VAR_EQU(&x, zprompt_text);
		mval_write(output, &var, TRUE);
	/* SV_ZROUTINES */
		if (!zro_root)
			zro_init();
		var.mvtype = MV_STR;
		var.str = dollar_zroutines;
		ZS_VAR_EQU(&x, zroutines_text);
		mval_write(output, &var, TRUE);
	/* SV_ZSOURCE */
		var.mvtype = MV_STR;
		var.str = dollar_zsource;
		ZS_VAR_EQU(&x, zsource_text);
		mval_write(output, &var, TRUE);
	/* SV_ZSTATUS */
		ZS_VAR_EQU(&x, zstatus_text);
		mval_write(output, &dollar_zstatus, TRUE);
	/* SV_ZSYSTEM */
		MV_FORCE_MVAL(&var, dollar_zsystem);
		ZS_VAR_EQU(&x, zsystem_text);
		mval_write(output, &var, TRUE);
	/* SV_ZTRAP */
		var.mvtype = MV_STR;
		var.str = dollar_ztrap.str;
		ZS_VAR_EQU(&x, ztrap_text);
		mval_write(output, &var, TRUE);
	/* SV_ZVERSION */
		var.mvtype = MV_STR;
		var.str.addr = &gtm_release_name[0];
		var.str.len = gtm_release_name_len;
		ZS_VAR_EQU(&x, zversion_text);
		mval_write(output, &var, TRUE);
	/* SV_ZYERROR */
		var.mvtype = MV_STR;
		var.str = dollar_zyerror.str;
		ZS_VAR_EQU(&x, zyerror_text);
		mval_write(output, &var, TRUE);
}
