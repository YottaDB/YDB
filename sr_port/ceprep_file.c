/****************************************************************
 *								*
 *	Copyright 2001, 2009 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#ifdef VMS
#include <rms.h>
static struct FAB	ceprep_fab;	/* file access block for compiler escape preprocessor output */
static struct RAB	ceprep_rab;	/* record access block for compiler escape preprocessor output */
#endif

#include "gtm_string.h"
#include "cmd_qlf.h"
#include "compiler.h"
#include "io.h"
#include "io_params.h"
#include "op.h"
#include "comp_esc.h"

#define CEPREP_OPEN_TIMEOUT 30

GBLREF unsigned char		source_buffer[];
GBLREF mident			module_name;
GBLREF io_pair			io_curr_device;
GBLREF command_qualifier	cmd_qlf;

static io_pair	dev_in_use;

void open_ceprep_file(void)
{
#ifdef VMS
/* stub except for VMS */
#ifdef __ALPHA
# pragma member_alignment save
# pragma nomember_alignment
#endif
	static readonly struct
	{
		unsigned char	newversion;
		unsigned char	wrap;
		unsigned char	width;
		int4		v_width;
		unsigned char	eol;
	} open_params_list =
	{
		(unsigned char)	iop_newversion,
		(unsigned char)	iop_wrap,
		(unsigned char)	iop_recordsize,
		(int4)		MAX_SRCLINE,
		(unsigned char)	iop_eol
	};
#ifdef __ALPHA
# pragma member_alignment restore
#endif
	int		mname_len;
	uint4		status;
	char		charspace, ceprep_name_buff[MAX_MIDENT_LEN + SIZEOF(".MCI") - 1], fname[255];
	mval		file, params;
	struct NAM	ceprep_nam;	/* name block for file access block for compiler escape preprocessor output */

	/* Create cepreprocessor output file. */
	ceprep_fab = cc$rms_fab;
	ceprep_fab.fab$l_dna = ceprep_name_buff;
	mname_len = module_name.len;
	assert(mname_len <= MAX_MIDENT_LEN);
	if (0 == mname_len)
	{
		MEMCPY_LIT(ceprep_name_buff, "MDEFAULT.MCI");
		ceprep_fab.fab$b_dns = SIZEOF("MDEFAULT.MCI") - 1;
	} else
	{
		memcpy(ceprep_name_buff, module_name.addr, mname_len);
		MEMCPY_LIT(&ceprep_name_buff[mname_len], ".MCI");
		ceprep_fab.fab$b_dns = mname_len + SIZEOF(".MCI") - 1;
	}
	if (MV_DEFINED(&cmd_qlf.ceprep_file))
	{
		ceprep_fab.fab$b_fns = cmd_qlf.ceprep_file.str.len;
		ceprep_fab.fab$l_fna = cmd_qlf.ceprep_file.str.addr;
	}
	ceprep_nam = cc$rms_nam;
	ceprep_nam.nam$l_esa = fname;
	ceprep_nam.nam$b_ess = SIZEOF(fname);
	ceprep_nam.nam$b_nop = (NAM$M_SYNCHK);
	ceprep_fab.fab$l_nam = &ceprep_nam;
	ceprep_fab.fab$l_fop = FAB$M_NAM;
	if (RMS$_NORMAL != (status = sys$parse(&ceprep_fab, 0, 0)))
		rts_error(VARLSTCNT(1) status);
	file.mvtype = params.mvtype = MV_STR;
	file.str.len = ceprep_nam.nam$b_esl;
	file.str.addr = fname;
	params.str.len = SIZEOF(open_params_list);
	params.str.addr = &open_params_list;
	op_open(&file, &params, CEPREP_OPEN_TIMEOUT, 0);
	params.str.len = 1;
	charspace = (char)iop_eol;
	params.str.addr = &charspace;
	dev_in_use = io_curr_device;
	op_use(&file, &params);
#endif
	return;
}

void close_ceprep_file(void)
{
#ifdef VMS
/* stub except for VMS */
	unsigned char	charspace;
	mval		param, ceprep_file;

	param.str.len = 1;
	charspace = (char)iop_eol;
	param.str.addr = &charspace;
	ceprep_file.mvtype = param.mvtype = MV_STR;
	ceprep_file.str.len = io_curr_device.in->trans_name->len;
	ceprep_file.str.addr = io_curr_device.in->trans_name->dollar_io;
	op_close(&ceprep_file, &param);
	io_curr_device = dev_in_use;
#endif
	return;
}

void put_ceprep_line(void)
{
#ifdef VMS
/* stub except for VMS */
	mval	out;

	out.mvtype = MV_STR;
	out.str.len = strlen((char *)source_buffer);
	out.str.addr = source_buffer;
	op_write(&out);
	op_wteol(1);
#endif
	return;
}
