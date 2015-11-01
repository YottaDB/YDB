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

#ifndef __CLI_H__
#define __CLI_H__

/*
 * -----------------------------------------------------------
 * Parser include file
 * -----------------------------------------------------------
 */
#define	MAX_PARMS	5	/* Maximum parameters on commands line */

#define MAX_CMD_LEN	25	/* Max Command name string length */
#define MAX_OPT_LEN	25	/* Max Option name string length */
#define MAX_ERR_STR	80	/* Max error string length */
#define MAX_LINE	32767+256	/* Max line len , maximum record size plus some overhead */

#define VAL_N_A		0	/* value type not applicable */
#define VAL_STR		1	/* String value type */
#define VAL_NUM		2	/* Number */
#define VAL_TIME	3	/* Time (can never be used on verb) */
#define VAL_LIST	3	/* Value can be a list
				 	(only used on verb, applies to last parameter) */

#define VAL_DCM		0	/* Number is Decimal */
#define VAL_HEX		1	/* Number is Hex */

#define VAL_DISALLOWED	0	/* Value Disallowed */
#define VAL_NOT_REQ	1	/* Value not Required, but allowed */
#define VAL_REQ		2	/* Value Required */


#define NON_NEG		0	/* Non Negatable */
#define NEG		1	/* Negatable */

#define CLI_PRESENT	1
#define CLI_NEGATED	2

/*
 * ------------------------------------------------------
 * Here the CLI_PARM structure is used
 * to give default values to a qualifier
 * wherever qualifiers dont require values.
 * Where qualifiers require values, the
 * CLI_PARM structure is used to prompt for the values.
 * ------------------------------------------------------
 */

typedef struct cmd_parm_struct {
	char		name[MAX_OPT_LEN];
	char		prompt[MAX_OPT_LEN];
} CLI_PARM;

typedef struct cmd_parm_tag {
	char		name[MAX_OPT_LEN];	/* name string */
	void		(*func)(void);		/* Ptr to worker function */
	struct cmd_parm_tag
			*parms;			/* Qualifiers */
	struct cmd_parm_struct
			*parm_values;		/* Parameters */
	unsigned 	required : 2;		/* Value required flag. Values :
							0 - disallowed,
							1 - optional
							2 - required */
	unsigned 	max_parms : 3;		/* Max. # of parameters allowed */
	unsigned 	negatable : 1;		/* Negatable flag  */
	unsigned 	val_type : 2;		/* Value Type
							VAL_N_A - type not applicable
							VAL_STR - String value type
							VAL_NUM - Number
							VAL_TIME - Time */
	unsigned 	hex_num : 1;		/* Number is hex */
	unsigned 	present : 1;		/* Arg. is present on command line */
	unsigned 	negated : 1;		/* Arg. negated on command line */
	char		*pval_str;		/* Value string */
} CLI_ENTRY;

typedef struct
{
	int     argc;

#ifdef __osf__
#pragma pointer_size (save)
#pragma pointer_size (long)
#endif

	char    **argv;

#ifdef __osf__
#pragma pointer_size (restore)
#endif

	char    in_str[MAX_LINE];       /* input string buffer */
	char    *tp;                    /* token pointer */
} IN_PARMS;

/* include platform independent prototypes */

#include "cliif.h"

void cli_strlwr(char *sp);
int cli_is_id(char *p);
void skip_white_space(void);
int cli_has_space(char *p);

#endif
