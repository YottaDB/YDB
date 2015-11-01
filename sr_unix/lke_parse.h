/****************************************************************
 *								*
 *	Copyright 2001, 2002 Sanchez Computer Associates, Inc.	*
 *								*
 *	This source code contains the intellectual property	*
 *	of its copyright holder(s), and is made available	*
 *	under a license.  If you do not know the terms of	*
 *	the license, please stop and do not read further.	*
 *								*
 ****************************************************************/

/*
 * -----------------------------------------------------------
 * Parser include file
 * -----------------------------------------------------------
 */

#define MAX_RN_LEN	15	/* Max Region name length */
#define MAX_CMD_LEN	10	/* Max Command name string length */
#define MAX_OPT_LEN	15	/* Max Option name string length */
#define MAX_ERR_STR	80	/* Max error string length */

#define N_A	0		/* value type not applicable */
#define STR	1		/* String value type */
#define NUM	2		/* Numeric */

#define NOT_REQ	0		/* Value not Required */
#define REQ	1		/* Value Required */

#define NON_NEG	0		/* Non Negatable */
#define NEG	1		/* Negatable */


typedef struct {
	char opt_name[MAX_OPT_LEN];	/* option name string */
	char *pval_str;		/* Value string */
	unsigned val_req : 1;	/* Value required flag */
	unsigned negatable : 1;	/* Negatable flag  */
	unsigned val_type : 2;	/* Value Type */
	unsigned present : 1;	/* Argument is present on command line */
	unsigned negated : 1;	/* Argument negated on command line */
} CMD_PARM;

typedef struct
{
	char		cmd[MAX_CMD_LEN];	/* Command name string */
	unsigned	arg_req : 1;		/* Argument required */
	CMD_PARM	*parms;			/* Parameters */
	int		(*func)();		/* Ptr to processing function */
} CMD_ENTRY;

