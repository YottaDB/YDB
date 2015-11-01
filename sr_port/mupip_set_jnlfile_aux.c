/****************************************************************
 *								*
 *	Copyright 2001 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#include "mdef.h"

#include "gtm_fcntl.h"
#include <unistd.h>
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"

#include "gdsroot.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsbt.h"
#include "gdsblk.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "cli.h"
#include "iosp.h"
#include "jnl.h"
#include "mupipset.h"
#include "mupint.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "mu_rndwn_file.h"
#ifdef VMS
#include "vmsfileexist.h"
#endif
#include "mu_gv_cur_reg_init.h"

GBLREF gd_region        *gv_cur_region;
GBLREF boolean_t	mu_rndwn_status;
GBLREF boolean_t	need_no_standalone;

#ifdef VMS
#define STANDALONE(x) mu_rndwn_file(TRUE)
#elif defined(UNIX)
#define STANDALONE(x) mu_rndwn_status = mu_rndwn_file(x, TRUE)
#else
#error unsupported platform
#endif

int4 mupip_set_jnlfile_aux(jnl_file_header *header)
{
	unsigned short	buf_len;
	unsigned int	full_buf_len, prev_buf_len;
	char		buf[JNL_NAME_SIZE], full_buf[JNL_NAME_SIZE], prev_buf[JNL_NAME_SIZE];
#ifdef VMS
	mstr		jnlfile, jnldef, *tmp;
#endif

	error_def(ERR_JNLFILNOTCHG);

	buf_len = sizeof(buf);
	/* check for standalone */
	need_no_standalone = cli_present("BYPASS");
	if (!need_no_standalone)
	{
		mu_gv_cur_reg_init();
		memcpy((char *)gv_cur_region->dyn.addr->fname, header->data_file_name, header->data_file_name_length);
		gv_cur_region->dyn.addr->fname_len = header->data_file_name_length;
		STANDALONE(gv_cur_region);
		if (FALSE == mu_rndwn_status)
		{
			util_out_print("could not get exclusive access to !AD", TRUE,
					header->data_file_name_length, header->data_file_name);
			return((int4)ERR_JNLFILNOTCHG);
		}
	}
	if (CLI_PRESENT == cli_present("PREVJNLFILE"))
	{
		if (!cli_get_str("PREVJNLFILE", buf, &buf_len))
		{
			util_out_print("Error : cannot get value for !AD",
					TRUE, LEN_AND_LIT("PREVJNLFILE"));
			return((int4)ERR_JNLFILNOTCHG);
		}
#ifdef VMS
		full_buf_len = buf_len;
		memcpy(full_buf, buf, buf_len);
		full_buf[buf_len] = '\0';
		if (0 != exp_conceal_path(buf, buf_len, full_buf, &full_buf_len))
		{
			util_out_print("!/Journal file !AD not created", TRUE, buf_len, buf);
			return((int4)ERR_JNLFILNOTCHG);
		}
		jnlfile.addr = full_buf;
		jnlfile.len = full_buf_len;
		jnldef.addr = JNL_EXT_DEF;
		jnldef.len = sizeof(JNL_EXT_DEF) - 1;
		if (!(tmp = vmsfileexist(&jnlfile,&jnldef)))
		{
			util_out_print("WARNING : Previous Journal file !AD for this journal does not exist, proceeding",
					TRUE, buf_len, buf);
		}
#elif defined(UNIX)
		if (!get_full_path(STR_AND_LEN(buf), full_buf, &full_buf_len, JNL_NAME_SIZE))
		{
			util_out_print("Error : cannot get the full path name", TRUE);
			return((int4)ERR_JNLFILNOTCHG);
		}
#else
#error Unsupported platform
#endif
		prev_buf_len = header->prev_jnl_file_name_length;
		if (header->prev_jnl_file_name_length != 0)
			memcpy(prev_buf, header->prev_jnl_file_name, prev_buf_len);
		else

			memcpy(prev_buf, "NULL", strlen(prev_buf));
		header->prev_jnl_file_name_length = full_buf_len;
		memcpy(header->prev_jnl_file_name, full_buf, full_buf_len);
		util_out_print("prev_jnl_file name changed from !AD to !AD", TRUE,
				prev_buf_len, prev_buf, header->prev_jnl_file_name_length, header->prev_jnl_file_name);
	} else if (CLI_NEGATED == cli_present("PREVJNLFILE"))
	{
		util_out_print("prev_jnl_file name changed from !AD to NULL", TRUE,
				header->prev_jnl_file_name_length, header->prev_jnl_file_name);
	 	memset(header->prev_jnl_file_name, 0, header->prev_jnl_file_name_length);
         	header->prev_jnl_file_name_length = (unsigned short)0;
	}
	if (CLI_PRESENT == cli_present("DBFILENAME"))
	{
		if (!cli_get_str("DBFILENAME", buf, &buf_len))
		{
			util_out_print("Error : cannot get value for !AD",
					TRUE, LEN_AND_LIT("DBFILENAME"));
			return((int4)ERR_JNLFILNOTCHG);
		}
#ifdef VMS
		full_buf_len = buf_len;
		memcpy(full_buf, buf, buf_len);
		full_buf[buf_len] = '\0';
		if (0 != exp_conceal_path(buf, buf_len, full_buf, &full_buf_len))
		{
			util_out_print("!/Journal file !AD not created", TRUE, buf_len, buf);
			return((int4)ERR_JNLFILNOTCHG);
		}
#endif
#ifdef UNIX
		if (!get_full_path(STR_AND_LEN(buf), full_buf, &full_buf_len, JNL_NAME_SIZE))
		{
			util_out_print("Error : cannot get the full path name", TRUE);
			return((int4)ERR_JNLFILNOTCHG);
		}
#endif
		if ((header->data_file_name_length == full_buf_len) &&
			(memcmp(full_buf, header->data_file_name, header->data_file_name_length) == 0))
		{
			util_out_print("Current generation Database !AD can not be changed _ Use mupip set journal option", TRUE,
						header->data_file_name_length, header->data_file_name);
			return((int4)ERR_JNLFILNOTCHG);
		}
		prev_buf_len = header->data_file_name_length;
		memcpy(prev_buf, header->data_file_name, prev_buf_len);
		memcpy(header->data_file_name, full_buf, full_buf_len);
		header->data_file_name_length = full_buf_len;
		util_out_print("data_file_name changed from !AD to !AD", TRUE,
				prev_buf_len, prev_buf, header->data_file_name_length, header->data_file_name);
	}
	if (CLI_PRESENT == cli_present("REPL_STATE"))
	{
		if (!cli_get_str("REPL_STATE", buf, &buf_len))
		{
			util_out_print("Error : cannot get value for qualifier REPL_STATE", TRUE);
			return((int4)ERR_JNLFILNOTCHG);
		}
		if (0 == STRCASECMP("ON", buf))
			header->repl_state = repl_open;
		else if (0 == STRCASECMP("OFF", buf))
			header->repl_state = repl_closed;
		else
		{
			util_out_print("Need either ON or OFF as REPL_STATE values.", TRUE);
			return((int4)ERR_JNLFILNOTCHG);
		}
		util_out_print("repl state changed to !AD", TRUE, buf_len, buf);
	}
	return SS_NORMAL;
}
