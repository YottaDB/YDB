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

#include "gtm_fcntl.h"
#include <unistd.h>
#include <errno.h>
#include "gtm_string.h"

#include <fab.h>
#include <iodef.h>
#include <nam.h>
#include <rmsdef.h>
#include <ssdef.h>
#include <efndef.h>

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
#include "gtm_file_stat.h"
#include "gtmmsg.h"

#define DB_EXT_DEF	".DAT"

int4 mupip_set_jnlfile(char *jnl_fname, int jnl_fn_len)
{
	int4		jnl_len, temp_jnl_fn_len;
	uint4		status;
	char		hdr_buffer[SIZEOF(jnl_file_header)];
	char		es_buffer[MAX_FN_LEN], name_buffer[MAX_FN_LEN], temp_jnl_fn[MAX_FN_LEN];
	jnl_file_header	*header;
	struct FAB	fab;
	struct NAM	nam;
	short		iosb[4];
	mstr 		dbfile, def;

	error_def(ERR_JNLFILNOTCHG);

	jnl_len = strlen(jnl_fname);
	temp_jnl_fn_len = jnl_len;
        memcpy(temp_jnl_fn, jnl_fname, jnl_len);
	temp_jnl_fn[jnl_len] = '\0';
	if (!get_full_path(temp_jnl_fn, temp_jnl_fn_len, jnl_fname, &jnl_len, jnl_fn_len, &status))
	{
		util_out_print("!/Unable to get full path file !AD", TRUE, temp_jnl_fn_len, temp_jnl_fn);
		gtm_putmsg(VARLSTCNT(1) status);
		return((int4)ERR_JNLFILNOTCHG);
	}
	nam = cc$rms_nam;
	nam.nam$l_rsa = name_buffer;
	nam.nam$b_rss = SIZEOF(name_buffer);
	nam.nam$l_esa = es_buffer;
	nam.nam$b_ess = SIZEOF(es_buffer);
	fab = cc$rms_fab;
	fab.fab$l_nam = &nam;
	fab.fab$l_fna = jnl_fname;
	fab.fab$b_fns = jnl_len;
	fab.fab$l_fop = FAB$M_UFO | FAB$M_MXV | FAB$M_CBT;
	fab.fab$b_fac = FAB$M_GET | FAB$M_PUT | FAB$M_BIO;
	fab.fab$l_dna = JNL_EXT_DEF;
	fab.fab$b_dns = SIZEOF(JNL_EXT_DEF) - 1;
	if ((status = sys$open(&fab))!= RMS$_NORMAL)
	{
		util_out_print("Error opening journal file !AD", TRUE, jnl_len, jnl_fname);
		if (0 != fab.fab$l_stv)
			gtm_putmsg(VARLSTCNT(3) status, 0, (uint4)fab.fab$l_stv);
		else
			gtm_putmsg(VARLSTCNT(1) status);
		return((int4)ERR_JNLFILNOTCHG);
	}
	/* Read Jnl Header */
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_READVBLK, &iosb[0], 0, 0,
					(sm_uc_ptr_t)hdr_buffer, SIZEOF(hdr_buffer), 1, 0, 0, 0);
	if (status == SS$_NORMAL)
		status = iosb[0];
	if (status != SS$_NORMAL)
	{
		util_out_print("Error reading file !AD", TRUE, jnl_len, jnl_fname);
		gtm_putmsg(VARLSTCNT(1) status);
		return((int4)ERR_JNLFILNOTCHG);
	}
	header = (jnl_file_header *)hdr_buffer;
        /* check if database is existing, warn if not  */
	dbfile.addr = header->data_file_name;
	dbfile.len = header->data_file_name_length;
	def.addr = DB_EXT_DEF;
	def.len = SIZEOF(DB_EXT_DEF)-1;
	if (FILE_PRESENT != gtm_file_stat(&dbfile, &def, NULL, FALSE, &status))
	{
                util_out_print("WARNING : Data base file !AD for this journal does not exist, proceeding",
                                TRUE, header->data_file_name_length, header->data_file_name);
		gtm_putmsg(VARLSTCNT(1) status);
	}
	/* Processing */
	if(SS_NORMAL != (status = mupip_set_jnlfile_aux(header, jnl_fname)))
		return status;
	/* Write Back Jnl Header */
	status = sys$qiow(EFN$C_ENF, fab.fab$l_stv, IO$_WRITEVBLK, &iosb[0], 0, 0,
					(sm_uc_ptr_t)hdr_buffer, SIZEOF(hdr_buffer), 1, 0, 0, 0);
	if (status == SS$_NORMAL)
		status = iosb[0];
	if (status != SS$_NORMAL)
	{
		util_out_print("Error writing file !AD", TRUE, jnl_len, jnl_fname);
		gtm_putmsg(VARLSTCNT(1) status);
		return((int4)ERR_JNLFILNOTCHG);
	}
	/* Close Jnl File */
	if(SS$_NORMAL != (status = sys$dassgn(fab.fab$l_stv)))
	{
		util_out_print("Error closing journal file !AD", jnl_len, jnl_fname);
		gtm_putmsg(VARLSTCNT(1) status);
		return((int4)ERR_JNLFILNOTCHG);
	}

	return SS_NORMAL;
}
