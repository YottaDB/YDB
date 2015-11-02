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


	JPDEF (jp_eol,			jpdt_nul),
	JPDEF (jp_account,		jpdt_nul),
#ifdef 	UNIX
	JPDEF (jp_cmdline,		jpdt_str),
#endif
	JPDEF (jp_default,		jpdt_str),
	JPDEF (jp_detached,		jpdt_nul),
	JPDEF (jp_error,		jpdt_str),
	JPDEF (jp_gbldir,		jpdt_str),
	JPDEF (jp_image,		jpdt_str),
	JPDEF (jp_input,		jpdt_str),
	JPDEF (jp_logfile,		jpdt_str),
	JPDEF (jp_noaccount,		jpdt_nul),
	JPDEF (jp_nodetached,		jpdt_nul),
	JPDEF (jp_noswapping,		jpdt_nul),
	JPDEF (jp_output,		jpdt_str),
	JPDEF (jp_priority,		jpdt_num),
	JPDEF (jp_process_name,		jpdt_str),
	JPDEF (jp_schedule,		jpdt_str),
	JPDEF (jp_startup,		jpdt_str),
	JPDEF (jp_swapping,		jpdt_nul),
	JPDEF (jp_nmvals,		jpdt_nul)

