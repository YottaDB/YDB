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


JPSDEF ( 3,  "ACC",	jp_account),		JPSDEF ( 8, "ACCOUNTI*",	jp_account),
#ifdef UNIX
JPSDEF ( 3,  "CMD",	jp_cmdline),		JPSDEF ( 7,  "CMDLINE",		jp_cmdline),
#endif
JPSDEF ( 3,  "DEF",	jp_default),		JPSDEF ( 7,  "DEFAULT",		jp_default),
JPSDEF ( 3,  "DET",	jp_detached),		JPSDEF ( 8,  "DETACHED",	jp_detached),
JPSDEF ( 3,  "ERR",	jp_error),		JPSDEF ( 5,  "ERROR",		jp_error),
JPSDEF ( 3,  "GBL",	jp_gbldir),		JPSDEF ( 6,  "GBLDIR",		jp_gbldir),
JPSDEF ( 2,  "IM",	jp_image),		JPSDEF ( 5,  "IMAGE",		jp_image),
JPSDEF ( 2,  "IN",	jp_input),		JPSDEF ( 5,  "INPUT",		jp_input),
JPSDEF ( 3,  "LOG",	jp_logfile),		JPSDEF ( 7,  "LOGFILE",		jp_logfile),
JPSDEF ( 5,  "NOACC",	jp_noaccount),		JPSDEF ( 8, "NOACCOUN*",	jp_noaccount),
JPSDEF ( 5,  "NODET",	jp_nodetached),		JPSDEF ( 8, "NODETACH*",	jp_nodetached),
JPSDEF ( 5,  "NOSWA",	jp_noswapping),		JPSDEF ( 8, "NOSWAPPI*",	jp_noswapping),
JPSDEF ( 3,  "OUT",	jp_output),		JPSDEF ( 6,  "OUTPUT",		jp_output),
JPSDEF ( 3,  "PRI",	jp_priority),		JPSDEF ( 8,  "PRIORITY",	jp_priority),
JPSDEF ( 3,  "PRO",	jp_process_name),	JPSDEF ( 7, "PROCESS*",		jp_process_name),
JPSDEF ( 3,  "SCH",	jp_schedule),		JPSDEF ( 8,  "SCHEDULE",	jp_schedule),
JPSDEF ( 3,  "STA",	jp_startup),		JPSDEF ( 7,  "STARTUP",		jp_startup),
JPSDEF ( 3,  "SWA",	jp_swapping),		JPSDEF ( 8,  "SWAPPING",	jp_swapping)

