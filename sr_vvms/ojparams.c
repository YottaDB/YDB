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

#include <ssdef.h>
#include <descrip.h>
#include <rmsdef.h>
#include <prcdef.h>
#include "job.h"
#include "min_max.h"

static readonly unsigned char definput[] = "NL:";
static readonly unsigned char deflogfile[] = "NL:";

static unsigned char *defoutbuf;
static unsigned char *deferrbuf;

LITREF	jp_datatype	job_param_datatypes[];
LITREF	mstr		define_gtm$job$_;
LITREF	mstr		set_default_;
LITREF	mstr		atsign;
LITREF	mstr		run__nodebug_;

error_def		(ERR_IVTIME);
error_def		(ERR_PRCNAMLEN);
error_def		(ERR_PARFILSPC);

void ojparams(unsigned char *p, mval *routine, bool *defprcnam, int4 *cmaxmsg, mstr *image,
        mstr *input, mstr *output, mstr *error, struct dsc$descriptor_s *prcnam, int4 *baspri,
        int4 *stsflg, mstr *gbldir, mstr *startup, struct dsc$descriptor_s *logfile, mstr *deffs,
        quadword *schedule)
{
	jp_type			ch;
	int4			status;
	struct dsc$descriptor_s	timdsc;
	int4			defstsflg;
	MSTR_CONST(defoutext, ".MJO");
	MSTR_CONST(deferrext, ".MJE");

/* Initializations */
	*defprcnam = FALSE;
	defstsflg = PRC$M_DETACH;
	*cmaxmsg = MAX(*cmaxmsg, (define_gtm$job$_.len + MAX_PIDSTR_LEN));
	image->len = 0;
	input->len = output->len = error->len = 0;
	prcnam->dsc$w_length	= 0;
	prcnam->dsc$b_dtype	= DSC$K_DTYPE_T;
	prcnam->dsc$b_class	= DSC$K_CLASS_S;
	*baspri = JP_NO_BASPRI;
	*stsflg = defstsflg;
	gbldir->len = 0;
	startup->len = 0;
	logfile->dsc$w_length	= 0;
	logfile->dsc$b_dtype	= DSC$K_DTYPE_T;
	logfile->dsc$b_class	= DSC$K_CLASS_S;
	deffs->len = 0;
	schedule->hi = schedule->lo = 0;

/* Process parameter list */
	while (*p != jp_eol)
	{
		switch (ch = *p++)
		{
		case jp_account:
 			*stsflg &= (~PRC$M_NOACNT);
			break;
		case jp_default:
			if (*p != 0)
			{
				deffs->len = (int)((unsigned char) *p);
				deffs->addr = (p + 1);
			}
			break;
		case jp_detached:
 			*stsflg |= PRC$M_DETACH;
			break;
		case jp_error:
			if (*p != 0)
			{
				error->len = (int)((unsigned char) *p);
				error->addr = (p + 1);
			}
			break;
		case jp_gbldir:
			if (*p != 0)
			{
				gbldir->len = (int)((unsigned char) *p);
				gbldir->addr = (p + 1);
			}
			break;
		case jp_image:
			if (*p != 0)
			{
				image->len = (int)((unsigned char) *p);
				image->addr = p + 1;
			}
			break;
		case jp_input:
			if (*p != 0)
			{
				input->len = (int)((unsigned char) *p);
				input->addr = p + 1;
			}
			break;
		case jp_logfile:
			if (*p != 0)
			{
				logfile->dsc$w_length = (int)((unsigned char) *p);
				logfile->dsc$a_pointer = p + 1;
			}
			break;
		case jp_noaccount:
			*stsflg |= PRC$M_NOACNT;
			break;
		case jp_nodetached:
 			*stsflg &= (~PRC$M_DETACH);
			break;
		case jp_noswapping:
			*stsflg |= PRC$M_PSWAPM;
			break;
		case jp_output:
			if (*p != 0)
			{
				output->len = (int)((unsigned char) *p);
				output->addr = p + 1;
			}
			break;
		case jp_priority:
			*baspri = *(int4 *)p;
			break;
		case jp_process_name:
			if (*p != 0)
			{
				prcnam->dsc$w_length = (int)((unsigned char) *p);
				prcnam->dsc$a_pointer = p + 1;
			}
			break;
		case jp_schedule:
			timdsc.dsc$w_length = (int)((unsigned char) *p);
			timdsc.dsc$b_dtype = DSC$K_DTYPE_T;
			timdsc.dsc$b_class = DSC$K_CLASS_S;
			timdsc.dsc$a_pointer = p + 1;
			status = sys$bintim (&timdsc, schedule);
			if (status != SS$_NORMAL)
				rts_error_csa(CSA_ARG(NULL) VARLSTCNT(4) ERR_IVTIME, 2, timdsc.dsc$w_length, timdsc.dsc$a_pointer);
			break;
		case jp_startup:
			if (*p != 0)
			{
				startup->len = (int)((unsigned char) *p);
				startup->addr = p + 1;
			}
			break;
		case jp_swapping:
			*stsflg &= (~PRC$M_PSWAPM);
			break;
		default:
			GTMASSERT;
		}
		switch (job_param_datatypes[ch])
		{
		case jpdt_nul:
			break;
		case jpdt_num:
			p += SIZEOF(int4);
			break;
		case jpdt_str:
			p += ((int)((unsigned char)*p)) + 1;
			break;
		default:
			GTMASSERT;
		}
	}

/* Defaults and Checks */
	if (image->len == 0)
		ojdefimage (image);
	else
		if ((status = ojchkfs (image->addr, image->len, TRUE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 5, "IMAGE", image->len, image->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, run__nodebug_.len + image->len);
	if (input->len == 0)
	{
		input->len = SIZEOF(definput) - 1;
		input->addr = definput;
	}
	else
		if ((status = ojchkfs (input->addr, input->len, TRUE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 5, "INPUT", input->len, input->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, 1 + input->len);
	if (output->len == 0)
	{
		if (!defoutbuf)
			defoutbuf = malloc(MAX_FILSPC_LEN);
		memcpy (&defoutbuf[0], routine->str.addr, routine->str.len);
		memcpy (&defoutbuf[routine->str.len], defoutext.addr, defoutext.len);
		if (*defoutbuf == '%')
			*defoutbuf = '_';
		output->len = routine->str.len + defoutext.len;
		output->addr = &defoutbuf[0];
	}
	else
		if ((status = ojchkfs (output->addr, output->len, FALSE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 6, "OUTPUT", output->len, output->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, 1 + output->len);
	if (error->len == 0)
	{
		if (!deferrbuf)
			deferrbuf = malloc(MAX_FILSPC_LEN);
		memcpy (&deferrbuf[0], routine->str.addr, routine->str.len);
		memcpy (&deferrbuf[routine->str.len], deferrext.addr, deferrext.len);
		if (*deferrbuf == '%')
			*deferrbuf = '_';
		error->len = routine->str.len + deferrext.len;
		error->addr = &deferrbuf[0];
	}
	else
		if ((status = ojchkfs (error->addr, error->len, FALSE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 5, "ERROR", error->len, error->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, 1 + error->len);

	if (prcnam->dsc$w_length > MAX_PRCNAM_LEN)
		rts_error_csa(CSA_ARG(NULL) VARLSTCNT(5)
				ERR_PRCNAMLEN, 3, prcnam->dsc$w_length, prcnam->dsc$a_pointer, MAX_PRCNAM_LEN);
	if (prcnam->dsc$w_length == 0)
	{
		ojdefprcnam (prcnam);
		*defprcnam = TRUE;
	}
	if (*baspri == JP_NO_BASPRI)
		ojdefbaspri (baspri);

	if (gbldir->len != 0)
		if ((status = ojchkfs (gbldir->addr, gbldir->len, FALSE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 6, "GBLDIR", gbldir->len, gbldir->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, 1 + gbldir->len);
	if (startup->len != 0)
		if ((status = ojchkfs (startup->addr, startup->len, TRUE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7)
					ERR_PARFILSPC, 4, 7, "STARTUP", startup->len, startup->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, atsign.len + startup->len);
	if (deffs->len == 0)
		ojdefdeffs (deffs);
	else
		if ((status = ojchkfs (deffs->addr, deffs->len, FALSE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 7, "DEFAULT", deffs->len, deffs->addr, status);
	*cmaxmsg = MAX(*cmaxmsg, set_default_.len + deffs->len);
	if (logfile->dsc$w_length == 0)
	{
		logfile->dsc$w_length = SIZEOF(deflogfile) - 1;
		logfile->dsc$a_pointer = deflogfile;
	}
	else
		if ((status = ojchkfs (logfile->dsc$a_pointer, logfile->dsc$w_length, FALSE)) != RMS$_NORMAL)
			rts_error_csa(CSA_ARG(NULL) VARLSTCNT(7) ERR_PARFILSPC, 4, 7, "LOGFILE", logfile->dsc$w_length,
				logfile->dsc$a_pointer, status);
	return;
}
