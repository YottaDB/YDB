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

/* maintained in conjunction with table in deviceparameters */

typedef struct {
	char offset;
	char letter;
}zshow_index;

enum zshow_params
{
zshow_allo,
zshow_bloc,
zshow_command,
zshow_conv,
zshow_ctra,
zshow_dele,
zshow_dest,
zshow_ebcd,
zshow_edit,
zshow_empterm,
zshow_exce,
zshow_exte,
zshow_field,
zshow_fil,
zshow_fixed,
zshow_follow,
zshow_host,
zshow_ichset,
zshow_independent,
zshow_inse,
zshow_lab,
zshow_leng,
zshow_nocene,
zshow_nodest,
zshow_noecho,
zshow_noedit,
zshow_noempterm,
zshow_noesca,
zshow_nofollow,
zshow_nohost,
zshow_noinse,
zshow_nopast,
zshow_noreads,
zshow_nottsy,
zshow_notype,
zshow_nowrap,
zshow_ochset,
zshow_pad,
zshow_parse,
zshow_past,
zshow_prmmbx,
zshow_rchk,
zshow_read,
zshow_reads,
zshow_rec,
zshow_shar,
zshow_shell,
zshow_stderr,
zshow_term,
zshow_ttsy,
zshow_type,
zshow_uic,
zshow_wait,
zshow_wchk,
zshow_width,
zshow_write
};
