/****************************************************************
 *								*
 *	Copyright 2001, 2011 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _REPL_ERRNO_H
#define _REPL_ERRNO_H

GBLREF int	repl_errno;

enum
{
	EREPL_JNLFILOPN				=  256,
	EREPL_SELECT,				/* 257 */
	EREPL_RECV,				/* 258 */
	EREPL_SEND,				/* 259 */
	EREPL_SEC_AHEAD,			/* 260 */
	EREPL_JNLFILESEEK,			/* 261 */
	EREPL_JNLFILEREAD,			/* 262 */
	EREPL_JNLRECINCMPL,			/* 263 */
	EREPL_JNLRECFMT,			/* 264 */
	EREPL_JNLBADALIGN,			/* 265 */
	EREPL_JNLEARLYEOF,			/* 266 */
	EREPL_BUFFNOTFRESH,			/* 267 */
	EREPL_LOGFILEOPEN,			/* 268 */
	EREPL_UPDSTART_SEMCTL,			/* 269 */
	EREPL_UPDSTART_FORK,			/* 270 */
	EREPL_UPDSTART_BADPATH,			/* 271 */
	EREPL_UPDSTART_EXEC,			/* 272 */
	EREPL_FILTERSTART_PIPE,			/* 273 */
	EREPL_FILTERSTART_NULLCMD,		/* 274 */
	EREPL_FILTERSTART_FORK,			/* 275 */
	EREPL_FILTERSTART_EXEC,			/* 276 */
	EREPL_FILTERSTART_FD2FP,		/* 277 */
	EREPL_FILTERNOTALIVE,			/* 278 */
	EREPL_FILTERSEND,			/* 279 */
	EREPL_FILTERBADCONV,			/* 280 */
	EREPL_INTLFILTER_NOSPC,			/* 281 */
	EREPL_INTLFILTER_BADREC,		/* 282 */
	EREPL_INTLFILTER_INCMPLREC,		/* 283 */
	EREPL_INTLFILTER_NEWREC,		/* 284 */
	EREPL_INTLFILTER_DATA2LONG,		/* 285 */
	EREPL_INTLFILTER_REPLGBL2LONG,		/* 286 */
	EREPL_FILTERRECV,			/* 287 */
	EREPL_FILTERNOSPC,			/* 288 */
	EREPL_INTLFILTER_SECNODZTRIGINTP,	/* 289 */
	EREPL_INTLFILTER_MULTILINEXECUTE,	/* 290 */
	EREPL_MAXERRNO				/* 291 */
};

#endif
