/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_string.h"
#include "gtm_limits.h"

#include <descrip.h>
#include <ssdef.h>
#include <rms.h>
#include <devdef.h>

#include "compiler.h"
#include "cmd_qlf.h"
#include "source_file.h"

GBLREF char			object_file_name[];
GBLREF char			rev_time_buf[];
GBLREF char			source_file_name[];
GBLREF unsigned char 		*source_buffer;
GBLREF short			object_name_len;
GBLREF unsigned short		source_name_len;
GBLREF command_qualifier	cmd_qlf;
GBLREF mident			routine_name, module_name;
GBLREF struct FAB		obj_fab;			/* file access block for the object file */

error_def(ERR_ERRORSUMMARY);
error_def(ERR_FILENOTFND);
error_def(ERR_OBJFILERR);
error_def(ERR_SRCFILERR);

static	bool			tt_so_do_once;
static struct FAB fab;
static struct RAB rab;

void compile_source_file(unsigned short flen, char *faddr, boolean_t mExtReqd /* not used in VMS */)
{
	struct FAB	srch_fab;
	struct NAM	srch_nam;
	char		exp_string_area[255], list_file[256], obj_file[256], ceprep_file[256];
	int		status;
	DCL_THREADGBL_ACCESS;

	SETUP_THREADGBL_ACCESS;
	obj_fab = cc$rms_fab;
	srch_fab = cc$rms_fab;
	srch_fab.fab$l_dna = DOTM;
	srch_fab.fab$b_dns = STR_LIT_LEN(DOTM);
	srch_fab.fab$l_fna = faddr;
	srch_fab.fab$b_fns = flen;
	srch_fab.fab$l_fop |= FAB$M_NAM;
	srch_fab.fab$l_nam = &srch_nam;
	srch_nam = cc$rms_nam;
	srch_nam.nam$l_rsa = source_file_name;
	srch_nam.nam$b_rss = NAME_MAX;		/* 255 since PATH_MAX is 256 on 7.3-2 */
	srch_nam.nam$l_esa = exp_string_area;
	srch_nam.nam$b_ess = SIZEOF(exp_string_area);
	status = sys$parse(&srch_fab);
	if (RMS$_NORMAL != status)
	{	dec_err(VARLSTCNT(4) ERR_SRCFILERR, 2, source_name_len, source_file_name);
		dec_err(VARLSTCNT(1) status);
		TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
	} else
	{
		cmd_qlf.object_file.str.addr = obj_file;
		cmd_qlf.object_file.str.len = 255;
		cmd_qlf.list_file.str.addr = list_file;
		cmd_qlf.list_file.str.len = 255;
		cmd_qlf.ceprep_file.str.addr = ceprep_file;
		cmd_qlf.ceprep_file.str.len = 255;
		get_cmd_qlf(&cmd_qlf);
		tt_so_do_once = FALSE;
		for (; ;)
		{
			if (srch_fab.fab$l_dev & DEV$M_FOD)
			{	status = sys$search(&srch_fab);
				if (status == RMS$_NMF )
				{	break;
				}
				else if (status == RMS$_FNF)
				{	dec_err(VARLSTCNT(4) ERR_FILENOTFND, 2, srch_nam.nam$b_esl, srch_nam.nam$l_esa);
					TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
					break;
				}
				else 	if (status != RMS$_NORMAL)
				{	dec_err(VARLSTCNT(4) ERR_SRCFILERR, 2, source_name_len, source_file_name);
					dec_err(VARLSTCNT(1) status);
					TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
					break;
				}
				else
				{	source_name_len = srch_nam.nam$b_rsl;
					source_file_name[source_name_len] = '\0';
				}
			} else
			{	source_name_len = SIZEOF("SYS$INPUT");
				memcpy(source_file_name, "SYS$INPUT", source_name_len);
				source_file_name[source_name_len] = '\0';
				tt_so_do_once = TRUE;
			}
			if (compiler_startup())
				TREF(dollar_zcstatus) = ERR_ERRORSUMMARY;
			else
			{
				status = sys$close(&obj_fab);
				obj_fab = cc$rms_fab;
				if (RMS$_NORMAL != status)
					rts_error(VARLSTCNT(6) ERR_OBJFILERR, 2, object_name_len, object_file_name, status,
						  obj_fab.fab$l_stv);
			}
			if (tt_so_do_once)
				break;
		}
	}
}

bool open_source_file(void)
{
	static readonly char inprompt[] = "\015\012>";
	struct NAM	nam;
	struct XABDAT	xab;
	char		exp_name[255];
	char		*p;
	int		n;
	int		rms_status;
	struct dsc$descriptor_s t_desc
		= {REV_TIME_BUFF_LEN, DSC$K_DTYPE_T, DSC$K_CLASS_S, rev_time_buf};

	fab = cc$rms_fab;
	fab.fab$l_fna = source_file_name;
	fab.fab$b_fns = source_name_len;
	fab.fab$w_mrs = MAX_SRCLINE;
	fab.fab$l_xab =	&xab;
	fab.fab$l_nam = &nam;
	nam = cc$rms_nam;
	nam.nam$l_esa = exp_name;
	nam.nam$b_ess = SIZEOF(exp_name);
	xab = cc$rms_xabdat;
	rms_status = sys$open(&fab);
	fab.fab$l_xab = 0;
	fab.fab$l_nam = 0;
	if (RMS$_NORMAL != rms_status)
	{
		dec_err(VARLSTCNT(4) ERR_SRCFILERR, 2, source_name_len, source_file_name);
		dec_err(VARLSTCNT(1) rms_status);
		return FALSE;
	}
	assert(tt_so_do_once || (source_name_len == nam.nam$b_esl && !memcmp(source_file_name, exp_name, nam.nam$b_esl)));
	rab = cc$rms_rab;
	rab.rab$l_fab = &fab;
	rab.rab$l_pbf = inprompt;
	rab.rab$b_psz = SIZEOF(inprompt) - 1;
	rab.rab$l_rop = RAB$M_PMT;
	rab.rab$l_ubf = source_buffer;
	rab.rab$w_usz = MAX_SRCLINE;
	rms_status = sys$connect(&rab);
	if (RMS$_NORMAL != rms_status)
	{
		dec_err(VARLSTCNT(4) ERR_SRCFILERR, 2, source_name_len, source_file_name);
		dec_err(VARLSTCNT(1) rms_status);
		return FALSE;
	}
	sys$asctim(0,&t_desc,&xab.xab$q_rdt,0);
	p = nam.nam$l_name ;
	n = nam.nam$b_name ;
	if (n > MAX_MIDENT_LEN)
		n = MAX_MIDENT_LEN;
	else if (!n)
	{
		p = "MDEFAULT";
		n = STR_LIT_LEN("MDEFAULT");
	}
	memcpy(routine_name.addr, p, n);
	memcpy(module_name.addr, p, n);
	routine_name.len = module_name.len = n;
	if  ('_' == *p)
		routine_name.addr[0] = '%';
	return TRUE;
}

int4 read_source_file(void)
{
	int rms_status;

	rms_status = sys$get(&rab);
	if (RMS$_EOF == rms_status)
		return -1;
	if (RMS$_NORMAL != rms_status)
		rts_error(VARLSTCNT(5) ERR_SRCFILERR, 2, source_name_len, source_file_name, rms_status);
	*(rab.rab$l_ubf + rab.rab$w_rsz + 1) = *(rab.rab$l_ubf + rab.rab$w_rsz)= 0;
	return rab.rab$w_rsz;
}

void close_source_file(void)
{
	sys$close(&fab);
	return;
}
