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

#include "gtm_string.h"

#include <rms.h>
#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsfhead.h"
#include "gdsblk.h"
#include "cli.h"
#include "dse.h"
#include "util.h"

#define NO_P_MSG_SIZE (SIZEOF(msg) / SIZEOF(int4) - 5)

static struct FAB patch_fab;
static struct RAB patch_rab;
static char patch_ofile[256];

GBLREF enum dse_fmt	dse_dmp_format;

void dse_open(void)
{
	$DESCRIPTOR(d_buff, patch_ofile);
	$DESCRIPTOR(d_ent, "FILE");
	int4 status,i;
	struct {
		short int arg_cnt;
		short int def_opt;
		int4 msg_number;
		short int fp_cnt;
		short new_opts;
		int4 fp_n[4];
	} msg;

	if (cli_present("FILE") == CLI_PRESENT)
	{
		if (CLOSED_FMT != dse_dmp_format)
		{
			util_out_print("Error:  output file already open.",TRUE);
			util_out_print("Current output file:  !AD", TRUE, strlen(patch_ofile), &patch_ofile[0]);
			return;
		}
		if (CLI$GET_VALUE(&d_ent,&d_buff) != SS$_NORMAL)
			return;
		for(i = 254; patch_ofile[i] == ' ' ;i--)
			if (i == 0)
			{
				util_out_print("Error: must specify a file name.",TRUE);
				return;
			}
		i++;
		patch_ofile[i] = 0;
		patch_fab = cc$rms_fab;
		patch_fab.fab$b_rat = FAB$M_CR;
		patch_fab.fab$l_fna = patch_ofile;
		patch_fab.fab$b_fns = i;
		status = sys$create(&patch_fab);
		switch (status )
		{
		case RMS$_NORMAL:
		case RMS$_CREATED:
		case RMS$_SUPERSEDE:
		case RMS$_FILEPURGED:
			break;
		default:
			msg.arg_cnt = NO_P_MSG_SIZE;
			msg.new_opts = msg.def_opt = 1;
			msg.msg_number = status;
			msg.fp_cnt = 0;
			sys$putmsg(&msg,0,0,0);
			return;
		}
		patch_rab = cc$rms_rab;
		patch_rab.rab$l_fab = &patch_fab;
		status = sys$connect(&patch_rab);
		if (status != RMS$_NORMAL)
		{
			msg.arg_cnt = NO_P_MSG_SIZE;
			msg.new_opts = msg.def_opt = 1;
			msg.msg_number = status;
			msg.fp_cnt = 0;
			sys$putmsg(&msg,0,0,0);
			return;
		}
		dse_dmp_format = OPEN_FMT;
	} else
	{
		if (CLOSED_FMT != dse_dmp_format)
			util_out_print("Current output file:  !AD", TRUE, strlen(patch_ofile), &patch_ofile[0]);
		else
			util_out_print("No current output file.",TRUE);
	}
	return;

}

boolean_t dse_fdmp_output(void *addr, int4 len)
{
	int4 status;
	static char	*buffer = NULL;
	static int	bufsiz = 0;

	struct {
		short int arg_cnt;
		short int def_opt;
		int4 msg_number;
		short int fp_cnt;
		short new_opts;
		int4 fp_n[4];
	} msg;
	assert(len >= 0);
	if (len + 1 > bufsiz)
	{
		if (buffer)
			free(buffer);
		bufsiz = len + 1;
		buffer = (char *)malloc(bufsiz);
	}
	if (len)
	{
		memcpy(buffer, addr, len);
		buffer[len] = 0;
	}
	patch_rab.rab$l_rbf = buffer;
	patch_rab.rab$w_rsz = len;
	status = sys$put(&patch_rab);
	if (status != RMS$_NORMAL)
	{
		rts_error(VARLSTCNT(1) status);
		return FALSE;
	}
	return TRUE;
}


void dse_close(void)
{
	int4 status;
	struct {
		short int arg_cnt;
		short int def_opt;
		int4 msg_number;
		short int fp_cnt;
		short new_opts;
		int4 fp_n[4];
	} msg;

	if (CLOSED_FMT != dse_dmp_format)
	{
		util_out_print("Closing output file:  !AD",TRUE,LEN_AND_STR(patch_ofile));
		status = sys$close(&patch_fab);
		if (status != RMS$_NORMAL)
		{
			msg.arg_cnt = NO_P_MSG_SIZE;
			msg.new_opts = msg.def_opt = 1;
			msg.msg_number = status;
			msg.fp_cnt = 0;
			sys$putmsg(&msg,0,0,0);
			return;
		}
		dse_dmp_format = CLOSED_FMT;
	}
	else
		util_out_print("Error:  no current output file.",TRUE);
	return;
}
