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

/* The third column represents the opcodes for functions to be used by op_indfun().
 * The one parameter version of $name can probably be folded into op_indfun, but at a later time.
 * Note: ***	Please add new entries to end of list so as not to cause execution problems for
 *		compilations from previous versions. Yes, they should have recompiled but we can
 *		avoid exploding by simply adding entries to end of list. ***
 * This comment and the preceeding empty lines put the first item at line 20, so adding 20 to an argcode
 * places you on the line with its information or subtracting 20 from a line gives the corresponding argcode
 */
INDIR(indir_fndata,		f_data,		OC_FNDATA)
,INDIR(indir_fnnext,		f_next,		OC_FNNEXT)
,INDIR(indir_fnorder1,		f_order1,	OC_FNORDER)
,INDIR(indir_get,		f_get1,		OC_FNGET)
,INDIR(indir_close,		m_close,	0)
,INDIR(indir_hang,		m_hang,		0)
,INDIR(indir_if,		m_if,		0)
,INDIR(indir_kill,		m_kill,		0)
,INDIR(indir_open,		m_open,		0)
,INDIR(indir_read,		m_read,		0)
,INDIR(indir_set,		m_set,		0)
,INDIR(indir_use,		m_use,		0)
,INDIR(indir_write,		m_write,	0)
,INDIR(indir_xecute,		m_xecute,	0)
,INDIR(indir_nref,		nref,		0)
,INDIR(indir_lock,		m_lock,		0)
,INDIR(indir_do,		m_do,		0)
,INDIR(indir_goto,		m_goto,		0)
,INDIR(indir_job,		m_job,		0)
,INDIR(indir_linetail,		linetail,	0)
,INDIR(indir_new,		m_new,		0)
,INDIR(indir_zlink,		m_zlink,	0)
,INDIR(indir_zbreak,		m_zbreak,	0)
,INDIR(indir_zsystem,		m_zsystem,	0)
,INDIR(indir_zedit,		m_zedit,	0)
,INDIR(indir_zmess,		m_zmessage,	0)
,INDIR(indir_zwatch,		m_zwatch,	0)
,INDIR(indir_zgoto,		m_zgoto,	0)
,INDIR(indir_zprint,		m_zprint,	0)
,INDIR(indir_zwrite,		m_zwrite,	0)
,INDIR(indir_glvn,		indirection,	0)
,INDIR(indir_lvadr,		indirection,	0)
,INDIR(indir_pattern,		indirection,	0)
,INDIR(indir_iset,		indirection,	0)
,INDIR(indir_text,		indirection,	0)
,INDIR(indir_view,		m_view,		0)
,INDIR(indir_zattach,		m_zattach,	0)
,INDIR(indir_zallocate,		m_zallocate,	0)
,INDIR(indir_zdeallocate,	m_zdeallocate,	0)
,INDIR(indir_lvarg,		indirection,	0)
,INDIR(indir_fnzprevious,	f_zprevious,	OC_FNZPREVIOUS)
,INDIR(indir_fnquery, 		f_query,	OC_FNQUERY)
,INDIR(indir_zhelp,		m_zhelp,	0)
,INDIR(indir_zshow,		m_zshow,	0)
,INDIR(indir_lvnamadr,		indirection,	0)
,INDIR(indir_zwithdraw,		m_zwithdraw,	0)
,INDIR(indir_tstart,		m_tstart,	0)
,INDIR(indir_fnname,		f_name,		0)		/* f_name is really a dummy */
,INDIR(indir_fnorder2,		f_order,	0)
,INDIR(indir_fnzqgblmod,	f_zqgblmod,	OC_FNZQGBLMOD)
,INDIR(indir_trollback,		m_trollback,	0)
,INDIR(indir_devparms,		indirection,	0)
,INDIR(indir_merge,		m_merge,	0)
,INDIR(indir_merge1,		m_merge,	0)
,INDIR(indir_merge2,		m_merge,	0)
,INDIR(indir_fntext,		f_text,		OC_FNTEXT)
,INDIR(indir_quit,		m_quit,		0)
,INDIR(indir_increment,		f_incr,		0)
,INDIR(indir_fnzahandle,	f_zahandle,	OC_FNZAHANDLE)
,INDIR(indir_fnzdata,		f_data,		OC_FNZDATA)
#ifdef GTM_TRIGGER
,INDIR(indir_ztrigger,		m_ztrigger,	0)
#endif
,INDIR(indir_zhalt,		m_zhalt,	0)
,INDIR(indir_fnzwrite,		f_zwrite,	OC_FNZWRITE)
,INDIR(indir_savglvn0,		indirection,	0)		/* this entry and the following use indirection as a dummy value */
,INDIR(indir_savlvn,		indirection,	0)
,INDIR(indir_savglvn1,		indirection,	0)		/* 0 and 1 (above) separate 2 variants of generated code */

