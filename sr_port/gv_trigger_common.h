/****************************************************************
 *								*
 *	Copyright 2010 Fidelity Information Services, Inc	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

#ifndef _GV_TRIGGER_COMMON_INCLUDED
#define _GV_TRIGGER_COMMON_INCLUDED
/* Following macros, though related to trigger global (^#t), are needed in trigger non-supported platforms.
 * While gv_trigger.h lives placed in sr_unix, VMS needs the following macros.
 */

#define	HASHT_GBL_CHAR1		'#'
#define	HASHT_GBL_CHAR2		't'
#define	HASHT_GBLNAME		"#t"
#define	HASHT_FULL_GBLNAME	"^#t"

#define	HASHT_GBLNAME_LEN	STR_LIT_LEN(HASHT_GBLNAME)
#define	HASHT_FULL_GBLNM_LEN	STR_LIT_LEN(HASHT_FULL_GBLNAME)
#define	HASHT_GBLNAME_FULL_LEN	STR_LIT_LEN(HASHT_GBLNAME) + 1	/* including terminating '\0' subscript */

/* This macro assumes addr points to a gv_currkey like subscript representation (with potential subscripts) */
#define	IS_GVKEY_HASHT_GBLNAME(LEN, ADDR)	((HASHT_GBLNAME_LEN <= LEN)				\
							&& (KEY_DELIMITER == ADDR[HASHT_GBLNAME_LEN])	\
							&& (HASHT_GBL_CHAR1 == ADDR[0])			\
							&& (HASHT_GBL_CHAR2 == ADDR[1]))

/* This macro assumes addr points to an mname (i.e. unsubscripted) */
#define	IS_MNAME_HASHT_GBLNAME(MNAME)	((HASHT_GBLNAME_LEN == MNAME.len) && !MEMCMP_LIT(MNAME.addr, HASHT_GBLNAME))

/* Similar to IS_GVKEY_HASHT_GBLNAME but used in places where ADDR points to ZWR formatted KEY (includes '^') */
#define IS_GVKEY_HASHT_FULL_GBLNAME(LEN, ADDR)	((HASHT_FULL_GBLNM_LEN <= LEN)				\
							&& ('^' == ADDR[0])				\
							&& (HASHT_GBL_CHAR1 == ADDR[1])			\
							&& (HASHT_GBL_CHAR2 == ADDR[2]))

#define	HASHT_GBL_CURLABEL	"2"	/* Currently supported ^#t global format */

/* HASHT_GBL_CURLABEL values of prior trigger versions */
#define V19_HASHT_GBL_LABEL	"1"	/* V5.4-000 to V5.4-001 */

#define	LITERAL_HASHLABEL	"#LABEL"
#define	LITERAL_HASHCYCLE	"#CYCLE"
#define	LITERAL_HASHCOUNT	"#COUNT"
#define	LITERAL_CMD		"CMD"
#define	LITERAL_GVSUBS		"GVSUBS"
#define	LITERAL_OPTIONS		"OPTIONS"
#define	LITERAL_DELIM		"DELIM"
#define	LITERAL_ZDELIM		"ZDELIM"
#define	LITERAL_PIECES		"PIECES"
#define	LITERAL_TRIGNAME	"TRIGNAME"
#define	LITERAL_XECUTE		"XECUTE"
#define	LITERAL_CHSET		"CHSET"

#define	LITERAL_HASHLABEL_LEN	STR_LIT_LEN(LITERAL_HASHLABEL)
#define	LITERAL_HASHCYCLE_LEN	STR_LIT_LEN(LITERAL_HASHCYCLE)
#define	LITERAL_HASHCOUNT_LEN	STR_LIT_LEN(LITERAL_HASHCOUNT)
#define	LITERAL_CMD_LEN		STR_LIT_LEN(LITERAL_CMD)
#define	LITERAL_GVSUBS_LEN	STR_LIT_LEN(LITERAL_GVSUBS)
#define	LITERAL_OPTIONS_LEN	STR_LIT_LEN(LITERAL_OPTIONS)
#define	LITERAL_DELIM_LEN	STR_LIT_LEN(LITERAL_DELIM)
#define	LITERAL_ZDELIM_LEN	STR_LIT_LEN(LITERAL_ZDELIM)
#define	LITERAL_PIECES_LEN	STR_LIT_LEN(LITERAL_PIECES)
#define	LITERAL_XECUTE_LEN	STR_LIT_LEN(LITERAL_XECUTE)
#define	LITERAL_TRIGNAME_LEN	STR_LIT_LEN(LITERAL_TRIGNAME)
#define	LITERAL_CHSET_LEN	STR_LIT_LEN(LITERAL_CHSET)

#endif
