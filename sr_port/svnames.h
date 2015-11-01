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

enum
{
	SV_HOROLOG = 1,		/* 1 */
	SV_IO,			/* 2 */
	SV_JOB,			/* 3 */
	SV_STORAGE,		/* 4 */
	SV_TEST,		/* 5 */
	SV_X,			/* 6 */
	SV_Y,			/* 7 */
	SV_ZIO,			/* 8 */
	SV_ZEOF,		/* 9 */
	SV_ZDIR,		/* 10 */
	SV_ZLEVEL,		/* 11 */
	SV_ZMODE,		/* 12 */
	SV_ZPROC,		/* 13 */
	SV_ZSTATUS,		/* 14 */
	SV_ZTRAP,		/* 15 */
	SV_ZA,			/* 16 */
	SV_ZB,			/* 17 */
	SV_ZC,			/* 18 */
	SV_ZVERSION,		/* 19 */
	SV_ZPOS,		/* 20 */
	SV_ZSOURCE,		/* 21 */
	SV_ZGBLDIR,		/* 22 */
	SV_ZROUTINES,		/* 23 */
	SV_ZCOMPILE,		/* 24 */
	SV_PRINCIPAL,		/* 25 */
	SV_ZCMDLINE,		/* 26 */
	SV_PROMPT,		/* 27 */
	SV_ZSYSTEM,		/* 28 */
	SV_KEY,			/* 29 */
	SV_DEVICE,		/* 30 */
	SV_ZCSTATUS,		/* 31 */
	SV_ZEDITOR,		/* 32 */
	SV_TRESTART,		/* 33 */
	SV_TLEVEL,		/* 34 */
	SV_ZSTEP,		/* 35 */
	SV_ZMAXTPTIME,		/* 36 */
	SV_QUIT,		/* 37 */
	SV_ECODE,		/* 38 */
	SV_ESTACK,		/* 39 */
	SV_ETRAP,		/* 40 */
	SV_STACK,		/* 41 */
	SV_ZERROR,		/* 42 */
	SV_ZYERROR,		/* 43 */
	SV_NUM_SV,		/* 44 */
	SV_DUMMY_TO_FORCE_INT = 0x0FFFFFFF	/* to ensure an int on S390 */
};
