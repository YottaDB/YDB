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

#include "gtm_fcntl.h"
#include "gtm_unistd.h"
#include <errno.h>
#include "gtm_stdio.h"
#include "gtm_string.h"
#include "gtm_strings.h"

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
#include "mupip_set.h"
#include "mupint.h"
#include "eintr_wrappers.h"
#include "util.h"
#include "mu_rndwn_file.h"
#include "gtm_file_stat.h"
#include "mu_gv_cur_reg_init.h"
#include "gtmmsg.h"

GBLREF gd_region        *gv_cur_region;
GBLREF boolean_t	need_no_standalone;

error_def(ERR_FILEEXISTS);
error_def(ERR_FILEPARSE);
error_def(ERR_JNLFILNOTCHG);
error_def(ERR_JNLFNF);
error_def(ERR_MUSTANDALONE);
error_def(ERR_PREVJNLLINKSET);

uint4 mupip_set_jnlfile_aux(jnl_file_header *header, char *jnl_fname)
{
	unsigned short	buf_len;
	unsigned int	full_buf_len, prev_buf_len, jnl_fn_len;
	uint4		ustatus;
	char		buf[JNL_NAME_SIZE], full_buf[JNL_NAME_SIZE], prev_buf[JNL_NAME_SIZE];
	char		jnl_fn[JNL_NAME_SIZE];
	mstr		jnlfile, jnldef;

	buf_len = SIZEOF(buf);
	/* check for standalone */
	need_no_standalone = cli_present("BYPASS");
	if (!need_no_standalone)
	{
		mu_gv_cur_reg_init();
		memcpy((char *)gv_cur_region->dyn.addr->fname, header->data_file_name, header->data_file_name_length);
		gv_cur_region->dyn.addr->fname_len = header->data_file_name_length;
		if (!STANDALONE(gv_cur_region))
		{
			gtm_putmsg(VARLSTCNT(4) ERR_MUSTANDALONE, 2, DB_LEN_STR(gv_cur_region));
			return((uint4)ERR_JNLFILNOTCHG);
		}
	}
	if (CLI_PRESENT == cli_present("PREVJNLFILE"))
	{
		if (!cli_get_str("PREVJNLFILE", buf, &buf_len))
		{
			util_out_print("Error : cannot get value for !AD",
					TRUE, LEN_AND_LIT("PREVJNLFILE"));
			return((uint4)ERR_JNLFILNOTCHG);
		}
		if (!get_full_path(STR_AND_LEN(buf), full_buf, &full_buf_len, SIZEOF(full_buf), &ustatus))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, LEN_AND_STR(buf), ustatus);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		if (!get_full_path(STR_AND_LEN(jnl_fname), jnl_fn, &jnl_fn_len, SIZEOF(jnl_fn), &ustatus))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, LEN_AND_STR(jnl_fname), ustatus);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		jnlfile.addr = full_buf;
		jnlfile.len = full_buf_len;
		jnldef.addr = JNL_EXT_DEF;
		jnldef.len = SIZEOF(JNL_EXT_DEF) - 1;
		if (FILE_PRESENT != gtm_file_stat(&jnlfile, &jnldef, NULL, FALSE, &ustatus))
			gtm_putmsg(VARLSTCNT(5) ERR_JNLFNF, 2, buf_len, buf, ustatus);
		if (jnl_fn_len == full_buf_len && (0 == memcmp(jnl_fn, full_buf, jnl_fn_len)))
		{
			util_out_print("Error : PREVJNLFILE !AD cannot link to itself", TRUE, jnl_fn_len, jnl_fn);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		prev_buf_len = header->prev_jnl_file_name_length;
		if (header->prev_jnl_file_name_length != 0)
			memcpy(prev_buf, header->prev_jnl_file_name, prev_buf_len);
		else
		{
			prev_buf_len = SIZEOF("NULL") - 1;
			memcpy(prev_buf, "NULL", prev_buf_len);
		}
		header->prev_jnl_file_name_length = full_buf_len;
		memcpy(header->prev_jnl_file_name, full_buf, full_buf_len);
		gtm_putmsg(VARLSTCNT(6) ERR_PREVJNLLINKSET, 4, prev_buf_len, prev_buf, header->prev_jnl_file_name_length,
			header->prev_jnl_file_name);
		send_msg(VARLSTCNT(6) ERR_PREVJNLLINKSET, 4, prev_buf_len, prev_buf, header->prev_jnl_file_name_length,
			header->prev_jnl_file_name);
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
			return((uint4)ERR_JNLFILNOTCHG);
		}
		if (!get_full_path(STR_AND_LEN(buf), full_buf, &full_buf_len, JNL_NAME_SIZE, &ustatus))
		{
			gtm_putmsg(VARLSTCNT(5) ERR_FILEPARSE, 2, LEN_AND_STR(buf), ustatus);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		if ((header->data_file_name_length == full_buf_len) &&
			(memcmp(full_buf, header->data_file_name, header->data_file_name_length) == 0))
		{
			util_out_print("Current generation Database !AD can not be changed _ Use mupip set journal option", TRUE,
						header->data_file_name_length, header->data_file_name);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		prev_buf_len = header->data_file_name_length;
		memcpy(prev_buf, header->data_file_name, prev_buf_len);
		memcpy(header->data_file_name, full_buf, full_buf_len);
		header->data_file_name_length = full_buf_len;
		header->data_file_name[full_buf_len] = '\0';	/* null terminate string just in case somebody cares */
		util_out_print("data_file_name changed from !AD to !AD", TRUE,
				prev_buf_len, prev_buf, header->data_file_name_length, header->data_file_name);
	}
	if (CLI_PRESENT == cli_present("REPL_STATE"))
	{
		if (!cli_get_str("REPL_STATE", buf, &buf_len))
		{
			util_out_print("Error : cannot get value for qualifier REPL_STATE", TRUE);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		if (0 == STRCASECMP("ON", buf))
			header->repl_state = repl_open;
		else if (0 == STRCASECMP("OFF", buf))
			header->repl_state = repl_closed;
		else
		{
			util_out_print("Need either ON or OFF as REPL_STATE values.", TRUE);
			return((uint4)ERR_JNLFILNOTCHG);
		}
		util_out_print("repl state changed to !AD", TRUE, buf_len, buf);
	}
	return SS_NORMAL;
}
