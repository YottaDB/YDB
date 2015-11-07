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

#include <ssdef.h>
#include <prtdef.h>
#include <prcdef.h>
#include <secdef.h>
#include <psldef.h>
#include <descrip.h>
#include <iodef.h>
#include <prvdef.h>
#include <lnmdef.h>
#include <rms.h>
#include <efndef.h>

#include "gtm_string.h"
#include "gdsroot.h"
#include "gdsbt.h"
#include "gtm_logicals.h"
#include "gtm_facility.h"
#include "fileinfo.h"
#include "gdsfhead.h"
#include "filestruct.h"
#include "iosp.h"
#include "vmsdtype.h"
#include "repl_shutdcode.h"
#include "repl_sp.h"
#include "cli.h"
#include "io.h"
#include "job.h"
#include "gtmmsg.h"
#include "trans_log_name.h"

#define MAX_MSG			1024
#define MAX_TRIES		50
#define MAX_PATH_LEN		255
#define MAX_MBX_NAME_LEN	255
#define SHORT_WAIT		10
#define TRY(X)			if (SS$_NORMAL != (status = X)) { sys$dassgn(*cmd_channel); return status; }

int4 repl_mbx_wr(uint4 channel, sm_uc_ptr_t msg, int len, uint4 err_code)
{
	int4		status;
	unsigned short	mbsb[4];

	error_def(ERR_TEXT);

	/* Qio's should be used with care, We should use qiow or wait properly to ensure that
		qio finished properly ( any arguments must not be on the stack before returning
		from the routine which issues qio) checking the iosb status block. */

	status = sys$qiow(EFN$C_ENF, channel, IO$_WRITEVBLK | IO$M_NOW, &mbsb[0], 0, 0, msg, len, 0, 0, 0, 0);
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(11) err_code, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to write to send-cmd mailbox the message, "),
					ERR_TEXT, 2, len, msg, status);
	}
	return status;
}

int4 repl_trnlnm(struct dsc$descriptor_s *d_tbl_srch_list, struct dsc$descriptor_s *d_logical,
		 struct dsc$descriptor_s *d_expanded, struct dsc$descriptor_s *d_foundin_tbl)
{
	struct
	{
		item_list_3	le[2];
		int4		terminator;
	} item_list;
	uint4 attr = LNM$M_CASE_BLIND;

	item_list.le[0].buffer_length		= LEN_OF_DSC(*d_expanded);
	item_list.le[0].item_code		= LNM$_STRING;
	item_list.le[0].buffer_address		= STR_OF_DSC(*d_expanded);
	item_list.le[0].return_length_address	= &(LEN_OF_DSC(*d_expanded));
	item_list.le[1].buffer_length		= LEN_OF_DSC(*d_foundin_tbl);
	item_list.le[1].item_code		= LNM$_TABLE;
	item_list.le[1].buffer_address		= STR_OF_DSC(*d_foundin_tbl);
	item_list.le[1].return_length_address	= &(LEN_OF_DSC(*d_foundin_tbl));
	item_list.terminator			= 0;
	return (sys$trnlnm(&attr, d_tbl_srch_list, d_logical, 0, &item_list));
}

int4 get_mbx_devname(struct dsc$descriptor_s *d_cmd_mbox, struct dsc$descriptor_s *d_cmd_dev)
{
	$DESCRIPTOR	(d_lnmtab, "LNM$TEMPORARY_MAILBOX");
	struct
	{
		item_list_3	le[1];
		int4		terminator;
	} item_list;

	item_list.le[0].buffer_length		= d_cmd_dev->dsc$w_length;
	item_list.le[0].item_code		= LNM$_STRING;
	item_list.le[0].buffer_address		= d_cmd_dev->dsc$a_pointer;
	item_list.le[0].return_length_address	= &(d_cmd_dev->dsc$w_length);
	item_list.terminator			= 0;
	return (sys$trnlnm(0, &d_lnmtab, d_cmd_mbox, 0, &item_list));
}

int4 repl_create_server(struct dsc$descriptor_s *d_cmd, char *mbx_prefix, char *mbx_suffix, uint4 *cmd_channel, uint4 *server_pid,
			uint4 err_code)
{
	int		cnt, i;
	int4		status;
	char		gbldir_path[MAX_PATH_LEN], gbldir_tbl[MAX_PATH_LEN], secshr_path[MAX_PATH_LEN], secshr_tbl[MAX_PATH_LEN];
	char		cmdmbox_devname[MAX_PATH_LEN], def_dir[MAX_PATH_LEN];
	char		creprc_log[MAX_PATH_LEN], creprc_log_exp[MAX_PATH_LEN];
	char		startup_cmd[MAX_PATH_LEN+1];
	char		cmdmbx_name[MAX_MBX_NAME_LEN + 1], trans_buff[MAX_FN_LEN];
	char		proc_name[PROC_NAME_MAXLEN + 1];
	mstr		image, log_nam, trans_log_nam;
	unsigned short	length;
	gds_file_id	file_id;

	static char		startup_file[MAX_PATH_LEN];
	static boolean_t	first_time = TRUE;

	$DESCRIPTOR(d_null_str, "");
	$DESCRIPTOR(d_loginout_image,"SYS$SYSTEM:LOGINOUT.EXE");
	$DESCRIPTOR(d_null_dev, "NL:");
	$DESCRIPTOR(d_secshr_logical, "GTMSECSHR");
	$DESCRIPTOR(d_gbldir_logical, "GTM$GBLDIR");
	$DESCRIPTOR(d_tbl_srch_list, "LNM$FILE_DEV");
	$DESCRIPTOR(d_startup_qualifier, "STARTUP_FILE");
	$DESCRIPTOR(d_cmd_dev, cmdmbox_devname);
	$DESCRIPTOR(d_out_dev, creprc_log);
	$DESCRIPTOR(d_out_dev_exp, creprc_log_exp);
	$DESCRIPTOR(d_def_dir, def_dir);
	$DESCRIPTOR(d_secshr, secshr_path);
	$DESCRIPTOR(d_gbldir, gbldir_path);
	$DESCRIPTOR(d_secshr_tbl, secshr_tbl);
	$DESCRIPTOR(d_gbldir_tbl, gbldir_tbl);
	$DESCRIPTOR(d_cmd_mbox, cmdmbx_name);
	$DESCRIPTOR(d_proc_name, proc_name);
	$DESCRIPTOR(d_startup_file, startup_file);
	$DESCRIPTOR(d_startup_cmd, startup_cmd);

	error_def(ERR_TEXT);
	error_def(ERR_MULOGNAMEDEF);
	error_def(ERR_MUNOACTION);

	/* Generate a unique name for the cmd_mbx using global_name(gbldir) and mbx_prefix*/
	log_nam.addr = GTM_GBLDIR;
	log_nam.len = SIZEOF(GTM_GBLDIR) - 1;
	if (SS_NORMAL != (status = trans_log_name(&log_nam, &trans_log_nam, trans_buff)))
	{
		gtm_putmsg(VARLSTCNT(6) err_code, 0, ERR_TEXT, 2, RTS_ERROR_LITERAL("gtm$gbldir not defined"));
		return status;
	}
	set_gdid_from_file((gd_id_ptr_t)&file_id, trans_buff, trans_log_nam.len);
	global_name(mbx_prefix, &file_id, cmdmbx_name);
	STR_OF_DSC(d_cmd_mbox)++;
        LEN_OF_DSC(d_cmd_mbox) = cmdmbx_name[0]; /* global_name() returns the length in the first byte */
	assert(SIZEOF(cmdmbx_name) > LEN_OF_DSC(d_cmd_mbox) + strlen(mbx_suffix));
	DSC_APND_STR(d_cmd_mbox, mbx_suffix);
	/* Prepare for the startup commands */
	if (RMS$_NORMAL != (status = parse_filename(&d_null_str, &d_def_dir, 0)))
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get value of default directory"), status);
		return status;
	}
	DSC_CPY(d_out_dev, d_def_dir);
	DSC_APND_LIT(d_out_dev, "loginout_");
	STR_OF_DSC(d_out_dev)[LEN_OF_DSC(d_out_dev)++] = STR_OF_DSC(d_cmd_mbox)[3];
	DSC_APND_LIT(d_out_dev, ".log");
	if ('\0' != mbx_suffix[0])
	{
		DSC_APND_LIT(d_out_dev, "_");
		DSC_APND_STR(d_out_dev, mbx_suffix);
	}
	if (RMS$_NORMAL != (status = parse_filename(&d_out_dev, &d_out_dev_exp, 1)))
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to expand creprc_logfile"), status);
		return status;
	}
	/* Create a mailbox to send commands to the detached 'loginout' process to start the server */
	/* Before creating the mailbox, check to see that the mail box's logical is not already defined.
	 * This is to take care of the cases where the previous incarnation of the server is just in the
	 * shutdown logic and the current server (which is just being brought up) gets to read from that
	 * server's mailbox (which contains just an 'exit' command). So, the current server fails to come up.
	 * As a side affect, this message also appears when a server is attempted to be started while
	 * another is already running from the same terminal.
	 */
	if (SS$_NORMAL ==  get_mbx_devname(&d_cmd_mbox, &d_cmd_dev))
	{
		gtm_putmsg(VARLSTCNT(6) err_code, 0, ERR_MULOGNAMEDEF, 2, LEN_STR_OF_DSC(d_cmd_mbox));
		return ERR_MUNOACTION;
	}
	status = sys$crembx(0, cmd_channel, MAX_MSG, MAX_MSG, 0, PSL$C_USER, &d_cmd_mbox);
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to create send-command mailbox"), status);
		return status;
	}
	/* Get the name of the send-command mailbox, which is needed for creprc() */
	for (cnt = 0; SS$_NORMAL != (status = get_mbx_devname(&d_cmd_mbox, &d_cmd_dev)) && (MAX_TRIES > cnt); cnt++)
	{
		SHORT_SLEEP(SHORT_WAIT);
	}
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get dev-name of send-command mailbox"), status);
		sys$dassgn(*cmd_channel);
		return status;
	}
	/* Construct process name */
	LEN_OF_DSC(d_proc_name) = (int) get_proc_name(STR_AND_LEN(mbx_prefix), getpid(), proc_name);
	/* Create the server as a detached process */
	status = sys$creprc(    server_pid,		/* process id */
				&d_loginout_image,	/* image */
				&d_cmd_dev,		/* input SYS$INPUT device */
				&d_out_dev_exp,		/* output SYS$OUTPUT device*/
				&d_out_dev_exp,		/* error SYS$ERROR device*/
				0, 0,
				&d_proc_name,		/* process name */
				0, 0, 0, PRC$M_DETACH | PRC$M_IMGDMP);
	if (SS$_NORMAL != status)
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to create detached server"), status);
		sys$dassgn(*cmd_channel);
		return status;
	}
	ojdefimage(&image);
	if (SS$_NORMAL != (status = repl_trnlnm(&d_tbl_srch_list, &d_secshr_logical, &d_secshr, &d_secshr_tbl)))
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to translate GTMSECSHR logical"), status);
		sys$dassgn(*cmd_channel);
		return status;
	}
	if (SS$_NORMAL != (status = repl_trnlnm(&d_tbl_srch_list, &d_gbldir_logical, &d_gbldir, &d_gbldir_tbl)))
	{
		gtm_putmsg(VARLSTCNT(7) err_code, 0,
				ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to translate GTM$GBLDIR logical"), status);
		sys$dassgn(*cmd_channel);
		return status;
	}
	if (CLI_PRESENT == cli_present("STARTUP_FILE"))
	{
		if (first_time && SS$_NORMAL != (status = cli$get_value(&d_startup_qualifier, &d_startup_file, &length)))
		{
			gtm_putmsg(VARLSTCNT(7) err_code, 0,
					ERR_TEXT, 2, RTS_ERROR_LITERAL("Unable to get value of /STARTUP_FILE qualifier"), status);
			sys$dassgn(*cmd_channel);
			return status;
		}
		first_time = FALSE;
		STR_OF_DSC(d_startup_cmd)[0] = '@';
		LEN_OF_DSC(d_startup_cmd) = 1;
		DSC_APND_DSC(d_startup_cmd, d_startup_file);
		TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_startup_cmd), err_code));
	}
	/* Write the mupip command (constructed earlier) to start the server into the send-command mailbox */
	TRY(repl_mbx_wr(*cmd_channel, LIT_AND_LEN("set default -"), err_code));
	TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_def_dir), err_code));
	if ((0 != memcmp(STR_OF_DSC(d_secshr_tbl), LIT_AND_LEN("LNM$GROUP"))) &&
	    (0 != memcmp(STR_OF_DSC(d_secshr_tbl), LIT_AND_LEN("LNM$SYSTEM_TABLE"))))
	{
		if (0 == memcmp(STR_OF_DSC(d_secshr_tbl), LIT_AND_LEN("LNM$JOB")))	/* chop the job specific suffix */
			LEN_OF_DSC(d_secshr_tbl) = SIZEOF("LNM$JOB") - 1;
		TRY(repl_mbx_wr(*cmd_channel, LIT_AND_LEN("define gtmsecshr /table= -"), err_code));
		STR_OF_DSC(d_secshr_tbl)[LEN_OF_DSC(d_secshr_tbl)] = ' ';
		STR_OF_DSC(d_secshr_tbl)[LEN_OF_DSC(d_secshr_tbl) +1 ] = '-';
		LEN_OF_DSC(d_secshr_tbl) += 2;
		TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_secshr_tbl), err_code));
		TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_secshr), err_code));
	}
	if ((0 != memcmp(STR_OF_DSC(d_gbldir_tbl), LIT_AND_LEN("LNM$GROUP"))) &&
	    (0 != memcmp(STR_OF_DSC(d_gbldir_tbl), LIT_AND_LEN("LNM$SYSTEM_TABLE"))))
	{
		if (0 == memcmp(STR_OF_DSC(d_gbldir_tbl), LIT_AND_LEN("LNM$JOB")))	/* chop the job specific suffix */
			LEN_OF_DSC(d_gbldir_tbl) = SIZEOF("LNM$JOB") - 1;
		TRY(repl_mbx_wr(*cmd_channel, LIT_AND_LEN("define gtm$gbldir /table= -"), err_code));
		STR_OF_DSC(d_gbldir_tbl)[LEN_OF_DSC(d_gbldir_tbl)] = ' ';
		STR_OF_DSC(d_gbldir_tbl)[LEN_OF_DSC(d_gbldir_tbl) +1 ] = '-';
		LEN_OF_DSC(d_gbldir_tbl) += 2;
		TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_gbldir_tbl), err_code));
		TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(d_gbldir), err_code));
	}
	TRY(repl_mbx_wr(*cmd_channel, LIT_AND_LEN("run -"), err_code));
	TRY(repl_mbx_wr(*cmd_channel, image.addr, image.len, err_code));
	TRY(repl_mbx_wr(*cmd_channel, STR_LEN_OF_DSC(*d_cmd), err_code));
	TRY(repl_mbx_wr(*cmd_channel, LIT_AND_LEN("exit"), err_code));
	return status;
}
