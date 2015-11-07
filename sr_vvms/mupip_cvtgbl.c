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

#include "gtm_string.h"

#include <rms.h>
#include <ssdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "muextr.h"
#include "util.h"
#include "io.h"
#include "mupip_exit.h"
#include "load.h"
#include "mu_outofband_setup.h"
#include "mupip_cvtgbl.h"
#include "trans_log_name.h"
#include "cli.h"

#define MAX_TRAN_NAM_LEN 257

GBLREF	gd_region	*gv_cur_region;
GBLREF	bool		mupip_error_occurred;
GBLREF	int		gv_fillfactor;
GBLREF	boolean_t	is_replicator;

error_def(ERR_LOADFMT);
error_def(ERR_LOADBGSZ);
error_def(ERR_LOADBGSZ2);
error_def(ERR_LOADEDSZ);
error_def(ERR_LOADEDSZ2);
error_def(ERR_LOADEDBG);
error_def(ERR_LOADFILERR);
error_def(ERR_MUPCLIERR);
error_def(ERR_MUNOACTION);
error_def(ERR_MUNOFINISH);

void mupip_cvtgbl(void)
{
	char		*c, *b, format_buffer[50], infilename[256];
	unsigned char	buf[MAX_TRAN_NAM_LEN];
	unsigned short	n_len;
	uint4		begin, end;
	uint4		stat;
	int		format, len, n;
	struct		FAB infab;
	struct		RAB inrab;
	struct XABPRO	xabpro;
	mstr		transed_file, untransed_file;
	gtm_int64_t	begin_i8, end_i8;

	is_replicator = TRUE;
	n_len = SIZEOF(format_buffer);
	if (0 == cli_get_str("FORMAT", format_buffer, &n_len))
	{
		n_len = SIZEOF("ZWR") - 1;
		memcpy(format_buffer, "ZWR", n_len);
		format = MU_FMT_ZWR;
	} else
	{
		if (0 == memcmp(format_buffer, "ZWR", n_len))
			format = MU_FMT_ZWR;
		else  if (0 == memcmp(format_buffer, "GO", n_len))
			format = MU_FMT_GO;
		else  if (0 == memcmp(format_buffer, "BINARY", n_len))
			format = MU_FMT_BINARY;
		else  if (0 == memcmp(format_buffer, "GOQ", n_len))
			format = MU_FMT_GOQ;
		else
			mupip_exit (ERR_LOADFMT);
	}
	mu_outofband_setup();
	mupip_error_occurred = FALSE;
	n_len = SIZEOF(infilename);
	if (0 == cli_get_str("FILE", infilename, &n_len))
		mupip_exit(ERR_MUPCLIERR);
	if (0 == cli_get_int("FILL_FACTOR", &gv_fillfactor))
		gv_fillfactor = MAX_FILLFACTOR;
	else if (gv_fillfactor > MAX_FILLFACTOR)
		gv_fillfactor = MAX_FILLFACTOR;
	else if (gv_fillfactor < MIN_FILLFACTOR)
		gv_fillfactor = MIN_FILLFACTOR;

	if (cli_get_int64("BEGIN", &begin_i8))
	{
		if (1 > begin_i8)
			mupip_exit(ERR_LOADBGSZ);
		else if (MAXUINT4 < begin_i8)
			mupip_exit(ERR_LOADBGSZ2);
		begin = begin_i8;
	} else
	{
		begin = 1;
		begin_i8 = 1;
	}
	if (cli_get_int64("END", &end_i8))
	{
		if (1 > end_i8)
			mupip_exit(ERR_LOADEDSZ);
		else if (MAXUINT4 < end_i8)
			mupip_exit(ERR_LOADEDSZ2);
		if (end_i8 < 1)
			mupip_exit(ERR_LOADEDSZ);
		if (end_i8 < begin_i8)
			mupip_exit(ERR_LOADEDBG);
		end = end_i8;
	} else
		end = MAXUINT4;
	gvinit();
	infab = cc$rms_fab;
	inrab = cc$rms_rab;
	inrab.rab$l_fab = &infab;
	untransed_file.addr = &infilename;
	untransed_file.len = n_len;
	switch(stat = trans_log_name(&untransed_file, &transed_file, buf))
	{
		case SS$_NORMAL:
			infab.fab$l_fna = transed_file.addr;
			infab.fab$b_fns = transed_file.len;
			break;
		case SS$_NOLOGNAM:
			infab.fab$l_fna = infilename;
			infab.fab$b_fns = n_len;
			break;
		default:
			mupip_exit(stat);
	}
	if (MU_FMT_GOQ == format)
	{
		infab.fab$l_fop = FAB$M_UFO;
		infab.fab$b_fac = FAB$M_BIO;
	} else
	{
		infab.fab$l_fop = FAB$M_SQO;
		infab.fab$b_fac = FAB$M_GET;
	}
	infab.fab$l_xab = &xabpro;
	xabpro = cc$rms_xabpro;
	inrab.rab$l_rop |= (RAB$M_LOC | RAB$M_RAH);
	inrab.rab$b_mbf = 20;
	stat = sys$open(&infab);
	if ((RMS$_NORMAL == stat) && (MU_FMT_GOQ != format))
		stat = sys$connect(&inrab);
	if (RMS$_NORMAL != stat)
	{
		rts_error(VARLSTCNT(8) ERR_LOADFILERR, 2, infab.fab$b_fns, infab.fab$l_fna, stat, 0, infab.fab$l_stv, 0);
		mupip_exit(ERR_MUNOACTION);
	}
	switch(format)
	{
		case MU_FMT_ZWR:
		case MU_FMT_GO:
			go_load(begin, end, &inrab, &infab);
			break;
		case MU_FMT_BINARY:
			bin_load(begin, end, &inrab, &infab);
			break;
		case MU_FMT_GOQ:
			goq_load(begin, end, &infab);
	}
	gv_cur_region = NULL;
	mupip_exit(mupip_error_occurred ? ERR_MUNOFINISH : SS$_NORMAL);
}
