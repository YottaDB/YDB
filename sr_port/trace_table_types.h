/****************************************************************
 *								*
 *	Copyright 2011, 2012 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/* Define trace groups/types -
 *
 * The actual order of these macros is not that important (groups are processed before trace types) but a logical
 * ordering is to define the group with a TRACEGROUP() macro ahead a list of related TRACETYPE() macros.
 *
 * Each trace type definition has the following 6 components (macro parameters):
 *
 *   1. Trace group name
 *   2. Trace type - is expanded as an enum name in tracetypes_enum.h.
 *   3. An integer (4 byte) field
 *   4. Address field 1 (size depends on platform)
 *   5. Address field 2
 *   6. Address field 3
 *
 * The address fields can of course contain ints but should be cast to INTPTR_T types in the
 * TRACE_ENTRY macro when used.
 *
 * Fields 3-6 are "text" fields that will be used by gtmpcat to identify the trace output.
 * If any field is not used, it should have a value of "". Suggest this file  be edited in a wide window
 */

/* iosocket_readfl trace group types */
TRACEGROUP(SOCKRFL)
TRACETYPE(SOCKRFL,	ENTRY,		"width",	"iod", 		"intrpt_cnt",	"")		/* Entry point */
TRACETYPE(SOCKRFL,	MVS_ZINTR,	"bytes_read",	"buffer_start",	"stp_free",	"")		/* Restart requested, mv_stent found*/
TRACETYPE(SOCKRFL,	RESTARTED,	"chars_read",	"max_bufflen",	"stp_need",	"buffer_start")	/* Restart proceeding, values prior to GC */
TRACETYPE(SOCKRFL,	RSTGC,		"",		"buffer_start",	"stp_free",	"")		/* Ditto, values after GC */
TRACETYPE(SOCKRFL,	BEGIN,		"chars_read",	"buffer_start",	"stp_free",	"")		/* Main line code values start of processing */
TRACETYPE(SOCKRFL,	OUTOFBAND,	"bytes_read",	"chars_read",	"buffer_start",	"")		/* Out-of-band recognized - interrupted */
TRACETYPE(SOCKRFL,	EXPBUFGC,	"bytes_read",	"stp_free",	"old_stp_free",	"max_bufflen")	/* Buffer expansion */
TRACETYPE(SOCKRFL,	RDSTATUS,	"read_status",	"out_of_band",	"out_of_time",	"")		/* Read results */
